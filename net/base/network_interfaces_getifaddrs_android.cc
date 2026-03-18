// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Taken from WebRTC's own implementation.
// https://webrtc.googlesource.com/src/+/4cad08ff199a46087f8ffe91ef89af60a4dc8df9/rtc_base/ifaddrs_android.cc

#include "net/base/network_interfaces_getifaddrs_android.h"

#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/scoped_generic.h"

namespace net::internal {

namespace {

struct netlinkrequest {
  nlmsghdr header;
  ifaddrmsg msg;
};

const int kMaxReadSize = 4096;

struct FdTraits {
  static int InvalidValue() { return -1; }

  static void Free(int f) { ::close(f); }
};

struct IfaddrsTraits {
  static struct ifaddrs* InvalidValue() { return nullptr; }

  static void Free(struct ifaddrs* ifaddrs) { Freeifaddrs(ifaddrs); }
};

int set_ifname(struct ifaddrs* ifaddr, int interface) {
  char buf[IFNAMSIZ] = {};
  char* name = if_indextoname(interface, buf);
  if (name == nullptr) {
    return -1;
  }
  ifaddr->ifa_name = new char[strlen(name) + 1];
  UNSAFE_TODO(strncpy(ifaddr->ifa_name, name, strlen(name) + 1));
  return 0;
}

int set_flags(struct ifaddrs* ifaddr) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    return -1;
  }
  ifreq ifr;
  UNSAFE_TODO(memset(&ifr, 0, sizeof(ifr)));
  UNSAFE_TODO(strncpy(ifr.ifr_name, ifaddr->ifa_name, IFNAMSIZ - 1));
  int rc = ioctl(fd, SIOCGIFFLAGS, &ifr);
  close(fd);
  if (rc == -1) {
    return -1;
  }
  ifaddr->ifa_flags = ifr.ifr_flags;
  return 0;
}

int set_addresses(struct ifaddrs* ifaddr,
                  ifaddrmsg* msg,
                  void* data,
                  size_t len) {
  if (msg->ifa_family == AF_INET) {
    if (len != sizeof(struct in_addr)) {
      DLOG(ERROR) << "Received an invalid length for an IPv4 address: " << len;
      return -1;
    }
    sockaddr_in* sa = new sockaddr_in;
    sa->sin_family = AF_INET;
    UNSAFE_TODO(memcpy(&sa->sin_addr, data, len));
    ifaddr->ifa_addr = reinterpret_cast<sockaddr*>(sa);
  } else if (msg->ifa_family == AF_INET6) {
    if (len != sizeof(struct in6_addr)) {
      DLOG(ERROR) << "Received an invalid length for an IPv6 address: " << len;
      return -1;
    }
    sockaddr_in6* sa = new sockaddr_in6;
    sa->sin6_family = AF_INET6;
    sa->sin6_scope_id = msg->ifa_index;
    UNSAFE_TODO(memcpy(&sa->sin6_addr, data, len));
    ifaddr->ifa_addr = reinterpret_cast<sockaddr*>(sa);
  } else {
    return -1;
  }
  return 0;
}

int make_prefixes(struct ifaddrs* ifaddr, int family, int prefixlen) {
  char* prefix = nullptr;
  if (family == AF_INET) {
    sockaddr_in* mask = new sockaddr_in;
    mask->sin_family = AF_INET;
    UNSAFE_TODO(memset(&mask->sin_addr, 0, sizeof(in_addr)));
    ifaddr->ifa_netmask = reinterpret_cast<sockaddr*>(mask);
    if (prefixlen > 32) {
      prefixlen = 32;
    }
    prefix = reinterpret_cast<char*>(&mask->sin_addr);
  } else if (family == AF_INET6) {
    sockaddr_in6* mask = new sockaddr_in6;
    mask->sin6_family = AF_INET6;
    UNSAFE_TODO(memset(&mask->sin6_addr, 0, sizeof(in6_addr)));
    ifaddr->ifa_netmask = reinterpret_cast<sockaddr*>(mask);
    if (prefixlen > 128) {
      prefixlen = 128;
    }
    prefix = reinterpret_cast<char*>(&mask->sin6_addr);
  } else {
    return -1;
  }
  for (int i = 0; i < (prefixlen / 8); i++) {
    UNSAFE_TODO(*prefix++) = 0xFF;
  }
  char remainder = 0xff;
  remainder <<= (8 - prefixlen % 8);
  *prefix = remainder;
  return 0;
}

int populate_ifaddrs(struct ifaddrs* ifaddr,
                     ifaddrmsg* msg,
                     void* bytes,
                     size_t len) {
  if (set_ifname(ifaddr, msg->ifa_index) != 0) {
    return -1;
  }
  if (set_flags(ifaddr) != 0) {
    return -1;
  }
  if (set_addresses(ifaddr, msg, bytes, len) != 0) {
    return -1;
  }
  if (make_prefixes(ifaddr, msg->ifa_family, msg->ifa_prefixlen) != 0) {
    return -1;
  }
  return 0;
}

// Deletes `sa` by casting to the appropriate pointer type. This is necessary
// because the gwp_asan memory checker verifies that the size matches the
// expected size, but these types are all different sizes.
void delete_sockaddr(sockaddr* sa) {
  if (!sa) {
    return;
  }
  switch (sa->sa_family) {
    case AF_INET:
      delete reinterpret_cast<sockaddr_in*>(sa);
      break;
    case AF_INET6:
      delete reinterpret_cast<sockaddr_in6*>(sa);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

int Getifaddrs(struct ifaddrs** result) {
  int fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    *result = nullptr;
    return -1;
  }

  base::ScopedGeneric<int, FdTraits> scoped_fd(fd);
  base::ScopedGeneric<struct ifaddrs*, IfaddrsTraits> scoped_ifaddrs;

  netlinkrequest ifaddr_request;
  UNSAFE_TODO(memset(&ifaddr_request, 0, sizeof(ifaddr_request)));
  ifaddr_request.header.nlmsg_flags = NLM_F_ROOT | NLM_F_REQUEST;
  ifaddr_request.header.nlmsg_type = RTM_GETADDR;
  ifaddr_request.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));

  ssize_t count = send(fd, &ifaddr_request, ifaddr_request.header.nlmsg_len, 0);
  if (static_cast<size_t>(count) != ifaddr_request.header.nlmsg_len) {
    close(fd);
    return -1;
  }
  struct ifaddrs* current = nullptr;
  char buf[kMaxReadSize];
  ssize_t amount_read = recv(fd, &buf, kMaxReadSize, 0);
  while (amount_read > 0) {
    nlmsghdr* header = reinterpret_cast<nlmsghdr*>(&buf[0]);
    size_t header_size = static_cast<size_t>(amount_read);
    for (; NLMSG_OK(header, header_size);
         header = UNSAFE_TODO(NLMSG_NEXT(header, header_size))) {
      switch (header->nlmsg_type) {
        case NLMSG_DONE:
          // Success. Return.
          *result = scoped_ifaddrs.release();
          return 0;
        case NLMSG_ERROR:
          *result = nullptr;
          return -1;
        case RTM_NEWADDR: {
          ifaddrmsg* address_msg =
              UNSAFE_TODO(reinterpret_cast<ifaddrmsg*>(NLMSG_DATA(header)));
          rtattr* rta = UNSAFE_TODO(IFA_RTA(address_msg));
          ssize_t payload_len = IFA_PAYLOAD(header);
          while (UNSAFE_TODO(RTA_OK(rta, payload_len))) {
            if ((address_msg->ifa_family == AF_INET &&
                 rta->rta_type == IFA_LOCAL) ||
                (address_msg->ifa_family == AF_INET6 &&
                 rta->rta_type == IFA_ADDRESS)) {
              ifaddrs* newest = new ifaddrs;
              UNSAFE_TODO(memset(newest, 0, sizeof(ifaddrs)));
              if (current) {
                current->ifa_next = newest;
              } else {
                scoped_ifaddrs.reset(newest);
              }
              if (populate_ifaddrs(newest, address_msg,
                                   UNSAFE_TODO(RTA_DATA(rta)),
                                   RTA_PAYLOAD(rta)) != 0) {
                *result = nullptr;
                return -1;
              }
              current = newest;
            }
            rta = UNSAFE_TODO(RTA_NEXT(rta, payload_len));
          }
          break;
        }
      }
    }
    amount_read = recv(fd, &buf, kMaxReadSize, 0);
  }
  *result = nullptr;
  return -1;
}

void Freeifaddrs(struct ifaddrs* addrs) {
  struct ifaddrs* last = nullptr;
  struct ifaddrs* cursor = addrs;
  while (cursor) {
    delete[] cursor->ifa_name;
    delete_sockaddr(cursor->ifa_addr);
    delete_sockaddr(cursor->ifa_netmask);
    last = cursor;
    cursor = cursor->ifa_next;
    delete last;
  }
}

}  // namespace net::internal
