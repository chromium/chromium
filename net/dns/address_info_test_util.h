// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ADDRESS_INFO_TEST_UTIL_H_
#define NET_DNS_ADDRESS_INFO_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

namespace net {
namespace test {

struct IpAndPort {
  struct Ip {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
  };
  Ip ip;
  int port;
};

// |N| is the length of the IpAndPort vector.
// (The templating greatly simplifies the internals of this function).
template <unsigned int N>
std::unique_ptr<char[]> make_addrinfo_list(std::vector<IpAndPort> ipp,
                                           const std::string& canonical_name);

std::unique_ptr<char[]> make_addrinfo(IpAndPort ipp,
                                      const std::string& canonical_name);

}  // namespace test
}  // namespace net

#endif  // NET_DNS_ADDRESS_INFO_TEST_UTIL_H_
