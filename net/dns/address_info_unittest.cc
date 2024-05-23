// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/address_info.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockAddrInfoGetter : public AddrInfoGetter {
 public:
  std::unique_ptr<addrinfo, FreeAddrInfoFunc> getaddrinfo(
      const std::string& host,
      const addrinfo* hints,
      int* out_os_error,
      handles::NetworkHandle network) override;

 private:
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

  // Initialises `addr` and `ai` from `ip_and_port`, `canonical_name` and
  // `ai_next`.
  static void InitializeAddrinfo(const IpAndPort& ip_and_port,
                                 char* canonical_name,
                                 addrinfo* ai_next,
                                 sockaddr_in* addr,
                                 addrinfo* ai);

  // Allocates and initialises an addrinfo structure containing the ip addresses
  // and ports from `ipp` and the name `canonical_name`. This function is
  // designed to be used within getaddrinfo(), which returns a raw pointer even
  // though it transfers ownership. So this function does the same. Since
  // addrinfo is a C-style variable-sized structure it cannot be allocated with
  // new. It is allocated with malloc() instead, so it must be freed with
  // free().
  template <size_t N>
  static std::unique_ptr<addrinfo, FreeAddrInfoFunc> MakeAddrInfoList(
      const IpAndPort (&ipp)[N],
      std::string_view canonical_name);

  static std::unique_ptr<addrinfo, FreeAddrInfoFunc> MakeAddrInfo(
      IpAndPort ipp,
      std::string_view canonical_name);
};

template <size_t N>
std::unique_ptr<addrinfo, FreeAddrInfoFunc>
MockAddrInfoGetter::MakeAddrInfoList(const IpAndPort (&ipp)[N],
                                     std::string_view canonical_name) {
  struct Buffer {
    addrinfo ai[N];
    sockaddr_in addr[N];
    char canonical_name[256];
  };

  CHECK_LE(canonical_name.size(), 255u);

  Buffer* const buffer = new Buffer();
  memset(buffer, 0x0, sizeof(Buffer));

  // At least one trailing nul byte on buffer->canonical_name was added by
  // memset() above.
  memcpy(buffer->canonical_name, canonical_name.data(), canonical_name.size());

  for (size_t i = 0; i < N; ++i) {
    InitializeAddrinfo(ipp[i], buffer->canonical_name,
                       i + 1 < N ? buffer->ai + i + 1 : nullptr,
                       buffer->addr + i, buffer->ai + i);
  }

  return {reinterpret_cast<addrinfo*>(buffer),
          [](addrinfo* ai) { delete reinterpret_cast<Buffer*>(ai); }};
}

std::unique_ptr<addrinfo, FreeAddrInfoFunc> MockAddrInfoGetter::MakeAddrInfo(
    IpAndPort ipp,
    std::string_view canonical_name) {
  return MakeAddrInfoList({ipp}, canonical_name);
}

void MockAddrInfoGetter::InitializeAddrinfo(const IpAndPort& ip_and_port,
                                            char* canonical_name,
                                            addrinfo* ai_next,
                                            sockaddr_in* addr,
                                            addrinfo* ai) {
  const uint8_t ip[4] = {ip_and_port.ip.a, ip_and_port.ip.b, ip_and_port.ip.c,
                         ip_and_port.ip.d};
  memcpy(&addr->sin_addr, ip, 4);
  addr->sin_family = AF_INET;
  addr->sin_port =
      base::HostToNet16(base::checked_cast<uint16_t>(ip_and_port.port));

  ai->ai_family = AF_INET;
  ai->ai_socktype = SOCK_STREAM;
  ai->ai_addrlen = sizeof(sockaddr_in);
  ai->ai_addr = reinterpret_cast<sockaddr*>(addr);
  ai->ai_canonname =
      reinterpret_cast<decltype(ai->ai_canonname)>(canonical_name);
  if (ai_next)
    ai->ai_next = ai_next;
}

std::unique_ptr<addrinfo, FreeAddrInfoFunc> MockAddrInfoGetter::getaddrinfo(
    const std::string& host,
    const addrinfo* /* hints */,
    int* out_os_error,
    handles::NetworkHandle) {
  // Presume success
  *out_os_error = 0;

  if (host == std::string("canonical.bar.com"))
    return MakeAddrInfo({{1, 2, 3, 4}, 80}, "canonical.bar.com");
  else if (host == "iteration.test")
    return MakeAddrInfoList({{{10, 20, 30, 40}, 80},
                             {{11, 21, 31, 41}, 81},
                             {{12, 22, 32, 42}, 82}},
                            "iteration.test");
  else if (host == "alllocalhost.com")
    return MakeAddrInfoList(
        {{{127, 0, 0, 1}, 80}, {{127, 0, 0, 2}, 80}, {{127, 0, 0, 3}, 80}},
        "alllocalhost.com");
  else if (host == "not.alllocalhost.com")
    return MakeAddrInfoList(
        {{{128, 0, 0, 1}, 80}, {{127, 0, 0, 2}, 80}, {{127, 0, 0, 3}, 80}},
        "not.alllocalhost.com");
  else if (host == "www.example.com")
    return MakeAddrInfo({{8, 8, 8, 8}, 80}, "www.example.com");

  // Failure
  *out_os_error = 1;

  return {nullptr, [](addrinfo*) {}};
}

std::unique_ptr<addrinfo> MakeHints(AddressFamily address_family,
                                    HostResolverFlags host_resolver_flags) {
  auto hints = std::make_unique<addrinfo>();
  *hints = {0};

  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      hints->ai_family = AF_INET;
      break;
    case ADDRESS_FAMILY_IPV6:
      hints->ai_family = AF_INET6;
      break;
    case ADDRESS_FAMILY_UNSPECIFIED:
      hints->ai_family = AF_UNSPEC;
      break;
  }

  if (host_resolver_flags & HOST_RESOLVER_CANONNAME)
    hints->ai_flags |= AI_CANONNAME;

  hints->ai_socktype = SOCK_STREAM;

  return hints;
}

TEST(AddressInfoTest, Failure) {
  auto getter = std::make_unique<MockAddrInfoGetter>();
  auto [ai, err, os_error] = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_NE(err, OK);
  EXPECT_NE(os_error, 0);
}

#if BUILDFLAG(IS_WIN)
// Note: this test is descriptive, not prescriptive.
TEST(AddressInfoTest, FailureWin) {
  auto getter = std::make_unique<MockAddrInfoGetter>();
  auto [ai, err, os_error] = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_EQ(err, ERR_NAME_RESOLUTION_FAILED);
  EXPECT_NE(os_error, 0);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Note: this test is descriptive, not prescriptive.
TEST(AddressInfoTest, FailureAndroid) {
  auto getter = std::make_unique<MockAddrInfoGetter>();
  auto [ai, err, os_error] = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_EQ(err, ERR_NAME_NOT_RESOLVED);
  EXPECT_NE(os_error, 0);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(AddressInfoTest, Canonical) {
  auto [ai, err, os_error] =
      AddressInfo::Get("canonical.bar.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_THAT(ai->GetCanonicalName(),
              std::optional<std::string>("canonical.bar.com"));
}

TEST(AddressInfoTest, Iteration) {
  auto [ai, err, os_error] =
      AddressInfo::Get("iteration.test",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);

  {
    int count = 0;
    for (const auto& addr_info : *ai) {
      const sockaddr_in* addr =
          reinterpret_cast<sockaddr_in*>(addr_info.ai_addr);
      EXPECT_EQ(base::HostToNet16(addr->sin_port) % 10, count % 10);
      ++count;
    }

    EXPECT_EQ(count, 3);
  }

  {
    int count = 0;
    for (auto&& aii : ai.value()) {
      const sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(aii.ai_addr);
      EXPECT_EQ(base::HostToNet16(addr->sin_port) % 10, count % 10);
      ++count;
    }

    EXPECT_EQ(count, 3);
  }
}

TEST(AddressInfoTest, IsAllLocalhostOfOneFamily) {
  auto [ai, err, os_error] =
      AddressInfo::Get("alllocalhost.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_TRUE(ai->IsAllLocalhostOfOneFamily());
}

TEST(AddressInfoTest, IsAllLocalhostOfOneFamilyFalse) {
  auto [ai, err, os_error] =
      AddressInfo::Get("not.alllocalhost.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_FALSE(ai->IsAllLocalhostOfOneFamily());
}

TEST(AddressInfoTest, CreateAddressList) {
  auto [ai, err, os_error] =
      AddressInfo::Get("www.example.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);

  AddressList list = ai->CreateAddressList();

  // Verify one result.
  ASSERT_EQ(1u, list.size());
  ASSERT_EQ(ADDRESS_FAMILY_IPV4, list[0].GetFamily());

  // Check if operator= works.
  AddressList copy;
  copy = list;
  ASSERT_EQ(1u, copy.size());
}

}  // namespace
}  // namespace net
