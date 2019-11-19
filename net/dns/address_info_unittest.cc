// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_info.h"

#include <memory>
#include <tuple>

#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/address_info_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockAddrInfoGetter : public AddrInfoGetter {
 public:
  addrinfo* getaddrinfo(const std::string& host,
                        const addrinfo* hints,
                        int* out_os_error) override;
  void freeaddrinfo(addrinfo* ai) override;
};

addrinfo* MockAddrInfoGetter::getaddrinfo(const std::string& host,
                                          const addrinfo* /* hints */,
                                          int* out_os_error) {
  // Presume success
  *out_os_error = 0;

  if (host == std::string("canonical.bar.com"))
    return reinterpret_cast<addrinfo*>(
        test::make_addrinfo({{1, 2, 3, 4}, 80}, "canonical.bar.com").release());
  else if (host == "iteration.test")
    return reinterpret_cast<addrinfo*>(
        test::make_addrinfo_list<3>({{{10, 20, 30, 40}, 80},
                                     {{11, 21, 31, 41}, 81},
                                     {{12, 22, 32, 42}, 82}},
                                    "iteration.test")
            .release());
  else if (host == "alllocalhost.com")
    return reinterpret_cast<addrinfo*>(
        test::make_addrinfo_list<3>(
            {{{127, 0, 0, 1}, 80}, {{127, 0, 0, 2}, 80}, {{127, 0, 0, 3}, 80}},
            "alllocalhost.com")
            .release());
  else if (host == "not.alllocalhost.com")
    return reinterpret_cast<addrinfo*>(
        test::make_addrinfo_list<3>(
            {{{128, 0, 0, 1}, 80}, {{127, 0, 0, 2}, 80}, {{127, 0, 0, 3}, 80}},
            "not.alllocalhost.com")
            .release());
  else if (host == "www.example.com")
    return reinterpret_cast<addrinfo*>(
        test::make_addrinfo({{8, 8, 8, 8}, 80}, "www.example.com").release());

  // Failure
  *out_os_error = 1;

  return nullptr;
}

void MockAddrInfoGetter::freeaddrinfo(addrinfo* ai) {
  std::unique_ptr<char[]> mock_addrinfo(reinterpret_cast<char*>(ai));
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
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  auto getter = std::make_unique<MockAddrInfoGetter>();
  std::tie(ai, err, os_error) = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_NE(err, OK);
  EXPECT_NE(os_error, 0);
}

#if defined(OS_WIN)
// Note: this test is descriptive, not prescriptive.
TEST(AddressInfoTest, FailureWin) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  auto getter = std::make_unique<MockAddrInfoGetter>();
  std::tie(ai, err, os_error) = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_EQ(err, ERR_NAME_RESOLUTION_FAILED);
  EXPECT_NE(os_error, 0);
}
#endif  // OS_WIN

#if defined(OS_ANDROID)
// Note: this test is descriptive, not prescriptive.
TEST(AddressInfoTest, FailureAndroid) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  auto getter = std::make_unique<MockAddrInfoGetter>();
  std::tie(ai, err, os_error) = AddressInfo::Get(
      "failure.com", *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
      std::move(getter));

  EXPECT_FALSE(ai);
  EXPECT_EQ(err, ERR_NAME_NOT_RESOLVED);
  EXPECT_NE(os_error, 0);
}
#endif  // OS_ANDROID

TEST(AddressInfoTest, Canonical) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  std::tie(ai, err, os_error) =
      AddressInfo::Get("canonical.bar.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_THAT(ai->GetCanonicalName(),
              base::Optional<std::string>("canonical.bar.com"));
}

TEST(AddressInfoTest, Iteration) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  std::tie(ai, err, os_error) =
      AddressInfo::Get("iteration.test",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);

  {
    int count = 0;
    for (auto aii = ai->begin(); aii != ai->end(); ++aii) {
      const sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(aii->ai_addr);
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
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  std::tie(ai, err, os_error) =
      AddressInfo::Get("alllocalhost.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_TRUE(ai->IsAllLocalhostOfOneFamily());
}

TEST(AddressInfoTest, IsAllLocalhostOfOneFamilyFalse) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  std::tie(ai, err, os_error) =
      AddressInfo::Get("not.alllocalhost.com",
                       *MakeHints(ADDRESS_FAMILY_IPV4, HOST_RESOLVER_CANONNAME),
                       std::make_unique<MockAddrInfoGetter>());

  EXPECT_TRUE(ai);
  EXPECT_EQ(err, OK);
  EXPECT_EQ(os_error, 0);
  EXPECT_FALSE(ai->IsAllLocalhostOfOneFamily());
}

TEST(AddressInfoTest, CreateAddressList) {
  base::Optional<AddressInfo> ai;
  int err;
  int os_error;
  std::tie(ai, err, os_error) =
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
