// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/public/resolv_reader.h"

#include <arpa/inet.h>
#include <resolv.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/sys_byteorder.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// MAXNS is normally 3, but let's test 4 if possible.
const char* const kNameserversIPv4[] = {
    "8.8.8.8",
    "192.168.1.1",
    "63.1.2.4",
    "1.0.0.1",
};

#if BUILDFLAG(IS_LINUX)
const char* const kNameserversIPv6[] = {
    nullptr,
    "2001:db8::42",
    nullptr,
    "::FFFF:129.144.52.38",
};
#endif

// Fills in |res| with sane configuration.
void InitializeResState(res_state res) {
  memset(res, 0, sizeof(*res));
  res->options = RES_INIT;

  for (unsigned i = 0; i < std::size(kNameserversIPv4) && i < MAXNS; ++i) {
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = base::HostToNet16(NS_DEFAULTPORT + i);
    inet_pton(AF_INET, kNameserversIPv4[i], &sa.sin_addr);
    res->nsaddr_list[i] = sa;
    ++res->nscount;
  }

#if BUILDFLAG(IS_LINUX)
  // Install IPv6 addresses, replacing the corresponding IPv4 addresses.
  unsigned nscount6 = 0;
  for (unsigned i = 0; i < std::size(kNameserversIPv6) && i < MAXNS; ++i) {
    if (!kNameserversIPv6[i])
      continue;
    // Must use malloc to mimic res_ninit. Expect to be freed in
    // `TestResolvReader::CloseResState()`.
    struct sockaddr_in6* sa6;
    sa6 = static_cast<sockaddr_in6*>(malloc(sizeof(*sa6)));
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = base::HostToNet16(NS_DEFAULTPORT - i);
    inet_pton(AF_INET6, kNameserversIPv6[i], &sa6->sin6_addr);
    res->_u._ext.nsaddrs[i] = sa6;
    memset(&res->nsaddr_list[i], 0, sizeof res->nsaddr_list[i]);
    ++nscount6;
  }
  res->_u._ext.nscount6 = nscount6;
#endif
}

void FreeResState(struct __res_state* res) {
#if BUILDFLAG(IS_LINUX)
  for (int i = 0; i < res->nscount; ++i) {
    if (res->_u._ext.nsaddrs[i] != nullptr)
      free(res->_u._ext.nsaddrs[i]);
  }
#endif
}

TEST(ResolvReaderTest, GetNameservers) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());

  std::optional<std::vector<IPEndPoint>> nameservers =
      GetNameservers(*res.get());
  EXPECT_TRUE(nameservers.has_value());

#if BUILDFLAG(IS_LINUX)
  EXPECT_EQ(kNameserversIPv4[0], nameservers->at(0).ToStringWithoutPort());
  EXPECT_EQ(kNameserversIPv6[1], nameservers->at(1).ToStringWithoutPort());
  EXPECT_EQ(kNameserversIPv4[2], nameservers->at(2).ToStringWithoutPort());
#else
  EXPECT_EQ(kNameserversIPv4[0], nameservers->at(0).ToStringWithoutPort());
  EXPECT_EQ(kNameserversIPv4[1], nameservers->at(1).ToStringWithoutPort());
  EXPECT_EQ(kNameserversIPv4[2], nameservers->at(2).ToStringWithoutPort());
#endif

  FreeResState(res.get());
}

}  // namespace

}  // namespace net
