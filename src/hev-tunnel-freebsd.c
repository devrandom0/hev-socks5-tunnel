/*
 ============================================================================
 Name        : hev-tunnel-freebsd.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2023 hev
 Description : Tunnel on FreeBSD
 ============================================================================
 */

#if defined(__FreeBSD__)

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if_tun.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>

#include <hev-task.h>
#include <hev-task-io.h>

#include "hev-tunnel.h"

static char tun_name[IFNAMSIZ];

int
hev_tunnel_open (const char *name)
{
    struct ifreq ifr;
    char path[256];
    int res = -1;
    int fd;

    snprintf (path, sizeof (path), "/dev/%s", name);
    fd = hev_task_io_open (path, O_RDWR);
    if (fd >= 0) {
        memcpy (tun_name, name, IFNAMSIZ);
        return fd;
    }

    fd = hev_task_io_open ("/dev/tun", O_RDWR);
    if (fd < 0)
        goto exit;

    res = ioctl (fd, TUNGIFNAME, (void *)&ifr);
    if (res < 0)
        goto exit_close;

    memcpy (tun_name, ifr.ifr_name, IFNAMSIZ);
    return fd;

exit_close:
    close (fd);
exit:
    return res;
}

void
hev_tunnel_close (int fd)
{
    struct ifreq ifr = { 0 };

    close (fd);

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return;

    memcpy (ifr.ifr_name, tun_name, IFNAMSIZ);
    ioctl (fd, SIOCIFDESTROY, (void *)&ifr);

    close (fd);
}

int
hev_tunnel_set_mtu (int mtu)
{
    int fd, res = -1;
    struct ifreq ifr = { .ifr_mtu = mtu };

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        goto exit;

    memcpy (ifr.ifr_name, tun_name, IFNAMSIZ);
    res = ioctl (fd, SIOCSIFMTU, (void *)&ifr);

    close (fd);
exit:
    return res;
}

int
hev_tunnel_set_state (int state)
{
    struct ifreq ifr = { 0 };
    int res = -1;
    int fd;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        goto exit;

    memcpy (ifr.ifr_name, tun_name, IFNAMSIZ);
    res = ioctl (fd, SIOCGIFFLAGS, (void *)&ifr);
    if (res < 0)
        goto exit_close;

    if (state)
        ifr.ifr_flags |= IFF_UP;
    else
        ifr.ifr_flags &= ~IFF_UP;
    res = ioctl (fd, SIOCSIFFLAGS, (void *)&ifr);

exit_close:
    close (fd);
exit:
    return res;
}

int
hev_tunnel_set_ipv4 (const char *addr, unsigned int prefix)
{
    struct ifaliasreq ifra = { 0 };
    struct sockaddr_in *pa;
    int res = -1;
    int fd;

    fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        goto exit;

    strcpy (ifra.ifra_name, tun_name);

    pa = (struct sockaddr_in *)&ifra.ifra_addr;
    pa->sin_len = sizeof (ifra.ifra_addr);
    pa->sin_family = AF_INET;
    res = inet_pton (AF_INET, addr, &pa->sin_addr);
    if (!res)
        goto exit_close;

    memcpy (&ifra.ifra_broadaddr, &ifra.ifra_addr, sizeof (ifra.ifra_addr));

    pa = (struct sockaddr_in *)&ifra.ifra_mask;
    pa->sin_len = sizeof (ifra.ifra_addr);
    pa->sin_family = AF_INET;
    pa->sin_addr.s_addr = htonl (((unsigned int)(-1)) << (32 - prefix));

    res = ioctl (fd, SIOCAIFADDR, &ifra);

exit_close:
    close (fd);
exit:
    return res;
}

int
hev_tunnel_set_ipv6 (const char *addr, unsigned int prefix)
{
    struct in6_aliasreq ifra = { .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME,
                                                    ND6_INFINITE_LIFETIME } };
    uint8_t *bytes;
    int res = -1;
    int fd;
    int i;

    fd = socket (AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
        goto exit;

    strcpy (ifra.ifra_name, tun_name);

    ifra.ifra_addr.sin6_len = sizeof (ifra.ifra_addr);
    ifra.ifra_addr.sin6_family = AF_INET6;
    res = inet_pton (AF_INET6, addr, &ifra.ifra_addr.sin6_addr);
    if (!res)
        goto exit_close;

    ifra.ifra_prefixmask.sin6_len = sizeof (ifra.ifra_prefixmask);
    ifra.ifra_prefixmask.sin6_family = AF_INET6;
    bytes = (uint8_t *)&ifra.ifra_prefixmask.sin6_addr;
    memset (bytes, 0xFF, 16);
    bytes[prefix / 8] <<= prefix % 8;
    prefix += prefix % 8;
    for (i = prefix / 8; i < 16; i++)
        bytes[i] = 0;

    res = ioctl (fd, SIOCAIFADDR_IN6, &ifra);

exit_close:
    close (fd);
exit:
    return res;
}

#endif /* __FreeBSD__ */
