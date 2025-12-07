// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_
#define NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdint.h>

#include <cstddef>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

bool operator==(const struct ifaddrmsg& lhs, const struct ifaddrmsg& rhs);

namespace net {
class IPAddress;
}

namespace net::test {

using NetlinkBuffer = std::vector<char>;

class NetlinkMessage {
 public:
  explicit NetlinkMessage(uint16_t type);
  ~NetlinkMessage();
  void AddPayload(base::span<const uint8_t> data);
  void AddAttribute(uint16_t type, base::span<const uint8_t> data);
  void AppendTo(NetlinkBuffer* output) const;

 private:
  void Append(base::span<const uint8_t> data);
  void Align();
  nlmsghdr* header() {
    return UNSAFE_TODO(reinterpret_cast<nlmsghdr*>(buffer_.data()));
  }

  NetlinkBuffer buffer_;
};

void MakeAddrMessageWithCacheInfo(uint16_t type,
                                  uint8_t flags,
                                  uint8_t family,
                                  int index,
                                  const IPAddress& address,
                                  const IPAddress& local,
                                  uint32_t preferred_lifetime,
                                  NetlinkBuffer* output);

void MakeAddrMessage(uint16_t type,
                     uint8_t flags,
                     uint8_t family,
                     int index,
                     const IPAddress& address,
                     const IPAddress& local,
                     NetlinkBuffer* output);

void MakeLinkMessage(uint16_t type,
                     uint32_t flags,
                     uint32_t index,
                     NetlinkBuffer* output,
                     bool clear_output = true);

void MakeWirelessLinkMessage(uint16_t type,
                             uint32_t flags,
                             uint32_t index,
                             NetlinkBuffer* output,
                             bool clear_output = true);
}  // namespace net::test

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_
