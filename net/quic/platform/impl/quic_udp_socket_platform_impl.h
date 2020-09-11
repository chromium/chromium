// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_UDP_SOCKET_PLATFORM_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_UDP_SOCKET_PLATFORM_IMPL_H_

#include <sys/socket.h>

#include <cstddef>
#include <cstdint>

namespace quic {

const size_t kCmsgSpaceForGooglePacketHeaderImpl = 0;

inline bool GetGooglePacketHeadersFromControlMessageImpl(
    struct ::cmsghdr* cmsg,
    char** packet_headers,
    size_t* packet_headers_len) {
  return false;
}

inline void SetGoogleSocketOptionsImpl(int fd) {}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_UDP_SOCKET_PLATFORM_IMPL_H_
