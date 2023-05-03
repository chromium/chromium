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
  void AddPayload(const void* data, size_t length);
  template <typename T>
  void AddPayload(const T& data) {
    AddPayload(&data, sizeof(data));
  }
  void AddAttribute(uint16_t type, const void* data, size_t length);
  void AppendTo(NetlinkBuffer* output) const;

 private:
  void Append(const void* data, size_t length);
  void Align();
  struct nlmsghdr* header() {
    return reinterpret_cast<struct nlmsghdr*>(buffer_.data());
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
