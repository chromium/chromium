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

namespace {

inline constexpr uint8_t kEcnMask = 0x03;

}  // namespace

const size_t kCmsgSpaceForGooglePacketHeaderImpl = 0;

inline bool GetGooglePacketHeadersFromControlMessageImpl(
    struct ::cmsghdr* cmsg,
    char** packet_headers,
    size_t* packet_headers_len) {
  return false;
}

inline void SetGoogleSocketOptionsImpl(int fd) {}

// Reads the current DSCP bits on the socket and adds the ECN field requested
// in |ecn_codepoint|. Sets |type| to be the correct cmsg_type to use in sendmsg
// to set the TOS byte, and writes a correctly formulated struct representing
// the TOS byte in |value|. (Some platforms deviate from the POSIX standard of
// IP_TOS/IPV6_TCLASS and an int).
// Returns 0 on success and an error code on failure.
inline int GetEcnCmsgArgsPreserveDscpImpl(const int fd,
                                          const int address_family,
                                          uint8_t ecn_codepoint,
                                          int& type,
                                          void* value,
                                          socklen_t& value_len) {
  // Return if the calling function did not provide a valid address family
  // or ECN codepoint.
  if ((address_family != AF_INET && address_family != AF_INET6) ||
      (ecn_codepoint & kEcnMask) != ecn_codepoint) {
    return -EINVAL;
  }
  if (value_len < sizeof(int)) {
    return -EINVAL;
  }
  int* arg = static_cast<int*>(value);
  if (getsockopt(fd, (address_family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6,
                 (address_family == AF_INET) ? IP_TOS : IPV6_TCLASS, arg,
                 &value_len) != 0) {
    return -1 * errno;
  }
  *arg &= static_cast<int>(~kEcnMask);
  *arg |= static_cast<int>(ecn_codepoint);
  type = (address_family == AF_INET) ? IP_TOS : IPV6_TCLASS;
  return 0;
}

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_UDP_SOCKET_PLATFORM_IMPL_H_
