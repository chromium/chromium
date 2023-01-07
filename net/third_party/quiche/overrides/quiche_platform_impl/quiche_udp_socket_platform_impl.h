// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_UDP_SOCKET_PLATFORM_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_UDP_SOCKET_PLATFORM_IMPL_H_

#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>

namespace quiche {

const size_t kCmsgSpaceForGooglePacketHeaderImpl = 0;

inline bool GetGooglePacketHeadersFromControlMessageImpl(
    struct ::cmsghdr* cmsg,
    char** packet_headers,
    size_t* packet_headers_len) {
  return false;
}

inline void SetGoogleSocketOptionsImpl(int fd) {}

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_UDP_SOCKET_PLATFORM_IMPL_H_
