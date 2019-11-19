// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "net/base/ip_address.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace net {
namespace {

const char kCanonicalHostname[] = "canonical.bar.com";

TEST(AddressListTest, Canonical) {
  // Create an addrinfo with a canonical name.
  struct sockaddr_in address;
  // The contents of address do not matter for this test,
  // so just zero-ing them out for consistency.
  memset(&address, 0x0, sizeof(address));
  // But we need to set the family.
  address.sin_family = AF_INET;
  struct addrinfo ai;
  memset(&ai, 0x0, sizeof(ai));
  ai.ai_family = AF_INET;
  ai.ai_socktype = SOCK_STREAM;
  ai.ai_addrlen = sizeof(address);
  ai.ai_addr = reinterpret_cast<sockaddr*>(&address);
  ai.ai_canonname = const_cast<char *>(kCanonicalHostname);

  // Copy the addrinfo struct into an AddressList object and
  // make sure it seems correct.
  AddressList addrlist1 = AddressList::CreateFromAddrinfo(&ai);
  EXPECT_EQ("canonical.bar.com", addrlist1.canonical_name());

  // Copy the AddressList to another one.
  AddressList addrlist2 = addrlist1;
  EXPECT_EQ("canonical.bar.com", addrlist2.canonical_name());
}

TEST(AddressListTest, CreateFromAddrinfo) {
  // Create an 4-element addrinfo.
  const unsigned kNumElements = 4;
  SockaddrStorage storage[kNumElements];
  struct addrinfo ai[kNumElements];
  for (unsigned i = 0; i < kNumElements; ++i) {
    struct sockaddr_in* addr =
        reinterpret_cast<struct sockaddr_in*>(storage[i].addr);
    storage[i].addr_len = sizeof(struct sockaddr_in);
    // Populating the address with { i, i, i, i }.
    memset(&addr->sin_addr, i, IPAddress::kIPv4AddressSize);
    addr->sin_family = AF_INET;
    // Set port to i << 2;
    addr->sin_port = base::HostToNet16(static_cast<uint16_t>(i << 2));
    memset(&ai[i], 0x0, sizeof(ai[i]));
    ai[i].ai_family = addr->sin_family;
    ai[i].ai_socktype = SOCK_STREAM;
    ai[i].ai_addrlen = storage[i].addr_len;
    ai[i].ai_addr = storage[i].addr;
    if (i + 1 < kNumElements)
      ai[i].ai_next = &ai[i + 1];
  }

  AddressList list = AddressList::CreateFromAddrinfo(&ai[0]);

  ASSERT_EQ(kNumElements, list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    EXPECT_EQ(ADDRESS_FAMILY_IPV4, list[i].GetFamily());
    // Only check the first byte of the address.
    EXPECT_EQ(i, list[i].address().bytes()[0]);
    EXPECT_EQ(static_cast<int>(i << 2), list[i].port());
  }

  // Check if operator= works.
  AddressList copy;
  copy = list;
  ASSERT_EQ(kNumElements, copy.size());

  // Check if copy is independent.
  copy[1] = IPEndPoint(copy[2].address(), 0xBEEF);
  // Original should be unchanged.
  EXPECT_EQ(1u, list[1].address().bytes()[0]);
  EXPECT_EQ(1 << 2, list[1].port());
}

TEST(AddressListTest, CreateFromIPAddressList) {
  struct TestData {
    std::string ip_address;
    const char* in_addr;
    int ai_family;
    size_t ai_addrlen;
    size_t in_addr_offset;
    size_t in_addr_size;
  } tests[] = {
    { "127.0.0.1",
      "\x7f\x00\x00\x01",
      AF_INET,
      sizeof(struct sockaddr_in),
      offsetof(struct sockaddr_in, sin_addr),
      sizeof(struct in_addr),
    },
    { "2001:db8:0::42",
      "\x20\x01\x0d\xb8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x42",
      AF_INET6,
      sizeof(struct sockaddr_in6),
      offsetof(struct sockaddr_in6, sin6_addr),
      sizeof(struct in6_addr),
    },
    { "192.168.1.1",
      "\xc0\xa8\x01\x01",
      AF_INET,
      sizeof(struct sockaddr_in),
      offsetof(struct sockaddr_in, sin_addr),
      sizeof(struct in_addr),
    },
  };
  const std::string kCanonicalName = "canonical.example.com";

  // Construct a list of ip addresses.
  IPAddressList ip_list;
  for (const auto& test : tests) {
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(test.ip_address));
    ip_list.push_back(ip_address);
  }

  AddressList test_list = AddressList::CreateFromIPAddressList(ip_list,
                                                               kCanonicalName);
  std::string canonical_name;
  EXPECT_EQ(kCanonicalName, test_list.canonical_name());
  EXPECT_EQ(base::size(tests), test_list.size());
}

TEST(AddressListTest, DeduplicatesEmptyAddressList) {
  AddressList empty;
  empty.Deduplicate();
  EXPECT_EQ(empty.size(), 0u);
}

TEST(AddressListTest, DeduplicatesSingletonAddressList) {
  AddressList singleton;
  singleton.push_back(IPEndPoint());
  singleton.Deduplicate();
  EXPECT_THAT(singleton.endpoints(), ElementsAre(IPEndPoint()));
}

TEST(AddressListTest, DeduplicatesLongerAddressList) {
  AddressList several;
  several.endpoints() = {IPEndPoint(IPAddress(0, 0, 0, 1), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 2), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 2), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 3), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 2), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 1), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 2), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 3), 0),
                         IPEndPoint(IPAddress(0, 0, 0, 2), 0)};
  several.Deduplicate();

  // Deduplication should preserve the order of the first instances
  // of the unique addresses.
  EXPECT_THAT(several.endpoints(),
              ElementsAre(IPEndPoint(IPAddress(0, 0, 0, 1), 0),
                          IPEndPoint(IPAddress(0, 0, 0, 2), 0),
                          IPEndPoint(IPAddress(0, 0, 0, 3), 0)));
}

}  // namespace
}  // namespace net
