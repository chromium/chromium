// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_socket_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string>

#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

using std::string;

namespace quic {

QuicMsgHdr::QuicMsgHdr(const char* buffer,
                       size_t buf_len,
                       const QuicSocketAddress& peer_address,
                       char* cbuf,
                       size_t cbuf_size)
    : iov_{const_cast<char*>(buffer), buf_len},
      cbuf_(cbuf),
      cbuf_size_(cbuf_size),
      cmsg_(nullptr) {
  // Only support unconnected sockets.
  DCHECK(peer_address.IsInitialized());

  raw_peer_address_ = peer_address.generic_address();
  hdr_.msg_name = &raw_peer_address_;
  hdr_.msg_namelen = raw_peer_address_.ss_family == AF_INET
                         ? sizeof(sockaddr_in)
                         : sizeof(sockaddr_in6);

  hdr_.msg_iov = &iov_;
  hdr_.msg_iovlen = 1;
  hdr_.msg_flags = 0;

  hdr_.msg_control = nullptr;
  hdr_.msg_controllen = 0;
}

void QuicMsgHdr::SetIpInNextCmsg(const QuicIpAddress& self_address) {
  if (!self_address.IsInitialized()) {
    return;
  }

  if (self_address.IsIPv4()) {
    QuicSocketUtils::SetIpInfoInCmsgData(
        self_address, GetNextCmsgData<in_pktinfo>(IPPROTO_IP, IP_PKTINFO));
  } else {
    QuicSocketUtils::SetIpInfoInCmsgData(
        self_address, GetNextCmsgData<in6_pktinfo>(IPPROTO_IPV6, IPV6_PKTINFO));
  }
}

void* QuicMsgHdr::GetNextCmsgDataInternal(int cmsg_level,
                                          int cmsg_type,
                                          size_t data_size) {
  // msg_controllen needs to be increased first, otherwise CMSG_NXTHDR will
  // return nullptr.
  hdr_.msg_controllen += CMSG_SPACE(data_size);
  DCHECK_LE(hdr_.msg_controllen, cbuf_size_);

  if (cmsg_ == nullptr) {
    DCHECK_EQ(nullptr, hdr_.msg_control);
    memset(cbuf_, 0, cbuf_size_);
    hdr_.msg_control = cbuf_;
    cmsg_ = CMSG_FIRSTHDR(&hdr_);
  } else {
    DCHECK_NE(nullptr, hdr_.msg_control);
    cmsg_ = CMSG_NXTHDR(&hdr_, cmsg_);
  }

  DCHECK_NE(nullptr, cmsg_) << "Insufficient control buffer space";

  cmsg_->cmsg_len = CMSG_LEN(data_size);
  cmsg_->cmsg_level = cmsg_level;
  cmsg_->cmsg_type = cmsg_type;

  return CMSG_DATA(cmsg_);
}

// static
void QuicSocketUtils::GetAddressAndTimestampFromMsghdr(
    struct msghdr* hdr,
    QuicIpAddress* address,
    QuicWallTime* walltimestamp) {
  if (hdr->msg_controllen > 0) {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(hdr); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(hdr, cmsg)) {
      char* addr_data = nullptr;
      int len = 0;
      if (cmsg->cmsg_type == IPV6_PKTINFO) {
        in6_pktinfo* info = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
        addr_data = reinterpret_cast<char*>(&info->ipi6_addr);
        len = sizeof(in6_addr);
        address->FromPackedString(addr_data, len);
      } else if (cmsg->cmsg_type == IP_PKTINFO) {
        in_pktinfo* info = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
        addr_data = reinterpret_cast<char*>(&info->ipi_addr);
        len = sizeof(in_addr);
        address->FromPackedString(addr_data, len);
      } else if (cmsg->cmsg_level == SOL_SOCKET &&
                 cmsg->cmsg_type == SO_TIMESTAMPING) {
        LinuxTimestamping* lts =
            reinterpret_cast<LinuxTimestamping*>(CMSG_DATA(cmsg));
        timespec* ts = &lts->systime;
        int64_t usec = (static_cast<int64_t>(ts->tv_sec) * 1000 * 1000) +
                       (static_cast<int64_t>(ts->tv_nsec) / 1000);
        *walltimestamp = QuicWallTime::FromUNIXMicroseconds(usec);
      }
    }
  }
}

// static
bool QuicSocketUtils::GetOverflowFromMsghdr(struct msghdr* hdr,
                                            QuicPacketCount* dropped_packets) {
  if (hdr->msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(hdr); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(hdr, cmsg)) {
      if (cmsg->cmsg_type == SO_RXQ_OVFL) {
        *dropped_packets = *(reinterpret_cast<uint32_t*> CMSG_DATA(cmsg));
        return true;
      }
    }
  }
  return false;
}

// static
bool QuicSocketUtils::GetTtlFromMsghdr(struct msghdr* hdr, int* ttl) {
  if (hdr->msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(hdr); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(hdr, cmsg)) {
      if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TTL) ||
          (cmsg->cmsg_level == IPPROTO_IPV6 &&
           cmsg->cmsg_type == IPV6_HOPLIMIT)) {
        *ttl = *(reinterpret_cast<int*>(CMSG_DATA(cmsg)));
        return true;
      }
    }
  }
  return false;
}

// static
int QuicSocketUtils::SetGetAddressInfo(int fd, int address_family) {
  int get_local_ip = 1;
  int rc = setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &get_local_ip,
                      sizeof(get_local_ip));
  if (rc == 0 && address_family == AF_INET6) {
    rc = setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &get_local_ip,
                    sizeof(get_local_ip));
  }
  return rc;
}

// static
int QuicSocketUtils::SetGetSoftwareReceiveTimestamp(int fd) {
  int timestamping = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
  return setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamping,
                    sizeof(timestamping));
}

// static
bool QuicSocketUtils::SetSendBufferSize(int fd, size_t size) {
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) != 0) {
    LOG(ERROR) << "Failed to set socket send size";
    return false;
  }
  return true;
}

// static
bool QuicSocketUtils::SetReceiveBufferSize(int fd, size_t size) {
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) != 0) {
    LOG(ERROR) << "Failed to set socket recv size";
    return false;
  }
  return true;
}

// static
int QuicSocketUtils::ReadPacket(int fd,
                                char* buffer,
                                size_t buf_len,
                                QuicPacketCount* dropped_packets,
                                QuicIpAddress* self_address,
                                QuicWallTime* walltimestamp,
                                QuicSocketAddress* peer_address) {
  DCHECK(peer_address != nullptr);
  char cbuf[kCmsgSpaceForReadPacket];

  iovec iov = {buffer, buf_len};
  struct sockaddr_storage raw_address;
  msghdr hdr;

  hdr.msg_name = &raw_address;
  hdr.msg_namelen = sizeof(sockaddr_storage);
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  struct cmsghdr* cmsg = reinterpret_cast<struct cmsghdr*>(cbuf);
  cmsg->cmsg_len = QUIC_ARRAYSIZE(cbuf);
  hdr.msg_control = cmsg;
  hdr.msg_controllen = QUIC_ARRAYSIZE(cbuf);

  int bytes_read = recvmsg(fd, &hdr, 0);

  // Return before setting dropped packets: if we get EAGAIN, it will
  // be 0.
  if (bytes_read < 0 && errno != 0) {
    if (errno != EAGAIN) {
      LOG(ERROR) << "Error reading " << strerror(errno);
    }
    return -1;
  }

  if (hdr.msg_flags & MSG_CTRUNC) {
    QUIC_BUG << "Incorrectly set control length: " << hdr.msg_controllen
             << ", expected " << QUIC_ARRAYSIZE(cbuf);
    return -1;
  }

  if (dropped_packets != nullptr) {
    GetOverflowFromMsghdr(&hdr, dropped_packets);
  }

  QuicIpAddress stack_address;
  if (self_address == nullptr) {
    self_address = &stack_address;
  }

  QuicWallTime stack_walltimestamp = QuicWallTime::FromUNIXMicroseconds(0);
  if (walltimestamp == nullptr) {
    walltimestamp = &stack_walltimestamp;
  }

  GetAddressAndTimestampFromMsghdr(&hdr, self_address, walltimestamp);

  *peer_address = QuicSocketAddress(raw_address);
  return bytes_read;
}

size_t QuicSocketUtils::SetIpInfoInCmsg(const QuicIpAddress& self_address,
                                        cmsghdr* cmsg) {
  string address_string;
  if (self_address.IsIPv4()) {
    cmsg->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;
    in_pktinfo* pktinfo = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
    memset(pktinfo, 0, sizeof(in_pktinfo));
    pktinfo->ipi_ifindex = 0;
    address_string = self_address.ToPackedString();
    memcpy(&pktinfo->ipi_spec_dst, address_string.c_str(),
           address_string.length());
    return sizeof(in_pktinfo);
  } else if (self_address.IsIPv6()) {
    cmsg->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    in6_pktinfo* pktinfo = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
    memset(pktinfo, 0, sizeof(in6_pktinfo));
    address_string = self_address.ToPackedString();
    memcpy(&pktinfo->ipi6_addr, address_string.c_str(),
           address_string.length());
    return sizeof(in6_pktinfo);
  } else {
    NOTREACHED() << "Unrecognized net::IPAddress";
    return 0;
  }
}

// static
WriteResult QuicSocketUtils::WritePacket(
    int fd,
    const char* buffer,
    size_t buf_len,
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address) {
  sockaddr_storage raw_address = peer_address.generic_address();
  iovec iov = {const_cast<char*>(buffer), buf_len};

  msghdr hdr;
  hdr.msg_name = &raw_address;
  hdr.msg_namelen = raw_address.ss_family == AF_INET ? sizeof(sockaddr_in)
                                                     : sizeof(sockaddr_in6);
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  const int kSpaceForIpv4 = CMSG_SPACE(sizeof(in_pktinfo));
  const int kSpaceForIpv6 = CMSG_SPACE(sizeof(in6_pktinfo));
  // kSpaceForIp should be big enough to hold both IPv4 and IPv6 packet info.
  const int kSpaceForIp =
      (kSpaceForIpv4 < kSpaceForIpv6) ? kSpaceForIpv6 : kSpaceForIpv4;
  char cbuf[kSpaceForIp];
  if (!self_address.IsInitialized()) {
    hdr.msg_control = nullptr;
    hdr.msg_controllen = 0;
  } else {
    hdr.msg_control = cbuf;
    hdr.msg_controllen = kSpaceForIp;
    cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
    SetIpInfoInCmsg(self_address, cmsg);
    hdr.msg_controllen = cmsg->cmsg_len;
  }

  int rc;
  do {
    rc = sendmsg(fd, &hdr, 0);
  } while (rc < 0 && errno == EINTR);
  if (rc >= 0) {
    return WriteResult(WRITE_STATUS_OK, rc);
  }
  return WriteResult((errno == EAGAIN || errno == EWOULDBLOCK)
                         ? WRITE_STATUS_BLOCKED
                         : WRITE_STATUS_ERROR,
                     errno);
}

// static
WriteResult QuicSocketUtils::WritePacket(int fd, const QuicMsgHdr& hdr) {
  int rc;
  do {
    rc = sendmsg(fd, hdr.hdr(), 0);
  } while (rc < 0 && errno == EINTR);
  if (rc >= 0) {
    return WriteResult(WRITE_STATUS_OK, rc);
  }
  return WriteResult((errno == EAGAIN || errno == EWOULDBLOCK)
                         ? WRITE_STATUS_BLOCKED
                         : WRITE_STATUS_ERROR,
                     errno);
}

// static
void QuicSocketUtils::SetIpInfoInCmsgData(const QuicIpAddress& self_address,
                                          void* cmsg_data) {
  DCHECK(self_address.IsInitialized());
  const std::string& address_str = self_address.ToPackedString();
  if (self_address.IsIPv4()) {
    in_pktinfo* pktinfo = static_cast<in_pktinfo*>(cmsg_data);
    pktinfo->ipi_ifindex = 0;
    memcpy(&pktinfo->ipi_spec_dst, address_str.c_str(), address_str.length());
  } else if (self_address.IsIPv6()) {
    in6_pktinfo* pktinfo = static_cast<in6_pktinfo*>(cmsg_data);
    memcpy(&pktinfo->ipi6_addr, address_str.c_str(), address_str.length());
  } else {
    QUIC_BUG << "Unrecognized IPAddress";
  }
}

// static
int QuicSocketUtils::CreateUDPSocket(const QuicSocketAddress& address,
                                     int32_t receive_buffer_size,
                                     int32_t send_buffer_size,
                                     bool* overflow_supported) {
  int address_family = address.host().AddressFamilyToInt();
  int fd = socket(address_family, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (fd < 0) {
    QUIC_LOG(ERROR) << "socket() failed: " << strerror(errno);
    return -1;
  }

  int get_overflow = 1;
  int rc = setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow,
                      sizeof(get_overflow));
  if (rc < 0) {
    QUIC_DLOG(WARNING) << "Socket overflow detection not supported";
  } else {
    *overflow_supported = true;
  }

  if (!SetReceiveBufferSize(fd, receive_buffer_size)) {
    return -1;
  }

  if (!SetSendBufferSize(fd, send_buffer_size)) {
    return -1;
  }

  rc = SetGetAddressInfo(fd, address_family);
  if (rc < 0) {
    LOG(ERROR) << "IP detection not supported" << strerror(errno);
    return -1;
  }

  rc = SetGetSoftwareReceiveTimestamp(fd);
  if (rc < 0) {
    QUIC_LOG(WARNING) << "SO_TIMESTAMPING not supported; using fallback: "
                      << strerror(errno);
  }

  return fd;
}

}  // namespace quic
