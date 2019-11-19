// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_linux_socket_utils.h"

#include <netinet/in.h>
#include <iostream>

#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"

namespace quic {

BufferedWrite::BufferedWrite(const char* buffer,
                             size_t buf_len,
                             const QuicIpAddress& self_address,
                             const QuicSocketAddress& peer_address)
    : BufferedWrite(buffer,
                    buf_len,
                    self_address,
                    peer_address,
                    std::unique_ptr<PerPacketOptions>()) {}

BufferedWrite::BufferedWrite(const char* buffer,
                             size_t buf_len,
                             const QuicIpAddress& self_address,
                             const QuicSocketAddress& peer_address,
                             std::unique_ptr<PerPacketOptions> options)
    : buffer(buffer),
      buf_len(buf_len),
      self_address(self_address),
      peer_address(peer_address),
      options(std::move(options)) {}

BufferedWrite::BufferedWrite(BufferedWrite&&) = default;

BufferedWrite::~BufferedWrite() = default;

QuicMMsgHdr::~QuicMMsgHdr() = default;

void QuicMMsgHdr::InitOneHeader(int i, const BufferedWrite& buffered_write) {
  mmsghdr* mhdr = GetMMsgHdr(i);
  msghdr* hdr = &mhdr->msg_hdr;
  iovec* iov = GetIov(i);

  iov->iov_base = const_cast<char*>(buffered_write.buffer);
  iov->iov_len = buffered_write.buf_len;
  hdr->msg_iov = iov;
  hdr->msg_iovlen = 1;
  hdr->msg_control = nullptr;
  hdr->msg_controllen = 0;

  // Only support unconnected sockets.
  DCHECK(buffered_write.peer_address.IsInitialized());

  sockaddr_storage* peer_address_storage = GetPeerAddressStorage(i);
  *peer_address_storage = buffered_write.peer_address.generic_address();
  hdr->msg_name = peer_address_storage;
  hdr->msg_namelen = peer_address_storage->ss_family == AF_INET
                         ? sizeof(sockaddr_in)
                         : sizeof(sockaddr_in6);
}

void QuicMMsgHdr::SetIpInNextCmsg(int i, const QuicIpAddress& self_address) {
  if (!self_address.IsInitialized()) {
    return;
  }

  if (self_address.IsIPv4()) {
    QuicSocketUtils::SetIpInfoInCmsgData(
        self_address, GetNextCmsgData<in_pktinfo>(i, IPPROTO_IP, IP_PKTINFO));
  } else {
    QuicSocketUtils::SetIpInfoInCmsgData(
        self_address,
        GetNextCmsgData<in6_pktinfo>(i, IPPROTO_IPV6, IPV6_PKTINFO));
  }
}

void* QuicMMsgHdr::GetNextCmsgDataInternal(int i,
                                           int cmsg_level,
                                           int cmsg_type,
                                           size_t data_size) {
  mmsghdr* mhdr = GetMMsgHdr(i);
  msghdr* hdr = &mhdr->msg_hdr;
  cmsghdr*& cmsg = *GetCmsgHdr(i);

  // msg_controllen needs to be increased first, otherwise CMSG_NXTHDR will
  // return nullptr.
  hdr->msg_controllen += CMSG_SPACE(data_size);
  DCHECK_LE(hdr->msg_controllen, cbuf_size_);

  if (cmsg == nullptr) {
    DCHECK_EQ(nullptr, hdr->msg_control);
    hdr->msg_control = GetCbuf(i);
    cmsg = CMSG_FIRSTHDR(hdr);
  } else {
    DCHECK_NE(nullptr, hdr->msg_control);
    cmsg = CMSG_NXTHDR(hdr, cmsg);
  }

  DCHECK_NE(nullptr, cmsg) << "Insufficient control buffer space";

  cmsg->cmsg_len = CMSG_LEN(data_size);
  cmsg->cmsg_level = cmsg_level;
  cmsg->cmsg_type = cmsg_type;

  return CMSG_DATA(cmsg);
}

int QuicMMsgHdr::num_bytes_sent(int num_packets_sent) {
  DCHECK_LE(0, num_packets_sent);
  DCHECK_LE(num_packets_sent, num_msgs_);

  int bytes_sent = 0;
  iovec* iov = GetIov(0);
  for (int i = 0; i < num_packets_sent; ++i) {
    bytes_sent += iov[i].iov_len;
  }
  return bytes_sent;
}

// static
int QuicLinuxSocketUtils::GetUDPSegmentSize(int fd) {
  int optval;
  socklen_t optlen = sizeof(optval);
  int rc = getsockopt(fd, SOL_UDP, UDP_SEGMENT, &optval, &optlen);
  if (rc < 0) {
    QUIC_LOG_EVERY_N_SEC(INFO, 10)
        << "getsockopt(UDP_SEGMENT) failed: " << strerror(errno);
    return -1;
  }
  QUIC_LOG_EVERY_N_SEC(INFO, 10)
      << "getsockopt(UDP_SEGMENT) returned segment size: " << optval;
  return optval;
}

}  // namespace quic
