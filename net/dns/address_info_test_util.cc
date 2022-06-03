// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_info_test_util.h"

#include <cstring>

#include "base/check.h"
#include "base/sys_byteorder.h"
#include "net/base/sys_addrinfo.h"

namespace net {
namespace test {

template <unsigned int N>
std::unique_ptr<char[]> make_addrinfo_list(std::vector<IpAndPort> ipp,
                                           const std::string& canonical_name) {
  struct Buffer {
    addrinfo ai[N];
    sockaddr_in addr[N];
    char canonical_name[256];
  };

  CHECK(ipp.size() == N);
  CHECK(canonical_name.length() <= 255);

  std::unique_ptr<char[]> data(new char[sizeof(Buffer)]);
  memset(data.get(), 0x0, sizeof(Buffer));
  Buffer* buffer = reinterpret_cast<Buffer*>(data.get());

  memcpy(&buffer->canonical_name[0], canonical_name.data(),
         canonical_name.length() + 1);

  for (unsigned int i = 0; i < N; ++i) {
    std::uint8_t ip[4] = {ipp[i].ip.a, ipp[i].ip.b, ipp[i].ip.c, ipp[i].ip.d};
    sockaddr_in* addr = &buffer->addr[i];
    memcpy(&addr->sin_addr, ip, 4);
    addr->sin_family = AF_INET;
    addr->sin_port = base::HostToNet16(static_cast<std::uint16_t>(ipp[i].port));

    addrinfo* ai = &buffer->ai[i];
    ai->ai_family = AF_INET;
    ai->ai_socktype = SOCK_STREAM;
    ai->ai_addrlen = sizeof(sockaddr_in);
    ai->ai_addr = reinterpret_cast<sockaddr*>(addr);
    ai->ai_canonname = reinterpret_cast<decltype(buffer->ai[0].ai_canonname)>(
        buffer->canonical_name);
    if (i < (N - 1))
      ai->ai_next = &buffer->ai[i + 1];
  }

  return data;
}

template std::unique_ptr<char[]> make_addrinfo_list<1>(
    std::vector<IpAndPort> ipp,
    const std::string& canonical_name);
template std::unique_ptr<char[]> make_addrinfo_list<3>(
    std::vector<IpAndPort> ipp,
    const std::string& canonical_name);

std::unique_ptr<char[]> make_addrinfo(IpAndPort ipp,
                                      const std::string& canonical_name) {
  return make_addrinfo_list<1>({ipp}, canonical_name);
}

}  // namespace test
}  // namespace net
