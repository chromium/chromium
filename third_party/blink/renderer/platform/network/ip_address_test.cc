// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/ip_address.h"

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink::test {

constexpr uint8_t kIpAddressBytes1[] = {192, 168, 1, 1};
constexpr uint8_t kIpAddressBytes2[] = {192, 168, 1, 2};
constexpr uint8_t kIpAddressBytes3[] = {200, 200, 200, 200};

TEST(BlinkIPAddressTest, HashTraits) {
  const net::IPAddress kIPAddr1(kIpAddressBytes1);
  const net::IPAddress kIPAddr2(kIpAddressBytes2);
  const net::IPAddress kEmptyIPAddr;

  net::IPAddress deleted_value;
  HashTraits<net::IPAddress>::ConstructDeletedValue(deleted_value);
  EXPECT_NE(deleted_value, kEmptyIPAddr);
  EXPECT_NE(deleted_value, kIPAddr1);
  EXPECT_NE(deleted_value, kIPAddr2);
  EXPECT_TRUE(
      WTF::IsHashTraitsDeletedValue<HashTraits<net::IPAddress>>(deleted_value));

  EXPECT_FALSE(
      WTF::IsHashTraitsDeletedValue<HashTraits<net::IPAddress>>(kEmptyIPAddr));
  EXPECT_FALSE(
      WTF::IsHashTraitsDeletedValue<HashTraits<net::IPAddress>>(kIPAddr1));
  EXPECT_FALSE(
      WTF::IsHashTraitsDeletedValue<HashTraits<net::IPAddress>>(kIPAddr2));

  EXPECT_TRUE(
      WTF::IsHashTraitsEmptyValue<HashTraits<net::IPAddress>>(kEmptyIPAddr));
  EXPECT_FALSE(
      WTF::IsHashTraitsEmptyValue<HashTraits<net::IPAddress>>(deleted_value));
  EXPECT_FALSE(
      WTF::IsHashTraitsEmptyValue<HashTraits<net::IPAddress>>(kIPAddr1));
  EXPECT_FALSE(
      WTF::IsHashTraitsEmptyValue<HashTraits<net::IPAddress>>(kIPAddr2));

  // Should be a 1 out of 4 billion chance these collide.
  EXPECT_NE(HashTraits<net::IPAddress>::GetHash(kIPAddr1),
            HashTraits<net::IPAddress>::GetHash(kIPAddr2));
}

TEST(BlinkIPAddressTest, HashIpAddress) {
  WTF::HashMap<net::IPAddress, int> ip_address_map;

  const net::IPAddress kIPAddr1(kIpAddressBytes1);
  const net::IPAddress kIPAddr2(kIpAddressBytes2);
  const net::IPAddress kIPAddr3(kIpAddressBytes3);

  ip_address_map.insert(kIPAddr1, 1);
  EXPECT_EQ(ip_address_map.size(), 1u);
  EXPECT_TRUE(ip_address_map.Contains(kIPAddr1));
  EXPECT_EQ(ip_address_map.at(kIPAddr1), 1);

  ip_address_map.insert(kIPAddr2, 2);
  EXPECT_EQ(ip_address_map.size(), 2u);

  ip_address_map.erase(kIPAddr1);
  EXPECT_FALSE(ip_address_map.Contains(kIPAddr1));
  EXPECT_EQ(ip_address_map.size(), 1u);

  ip_address_map.insert(kIPAddr3, 2);
  EXPECT_EQ(ip_address_map.size(), 2u);
}

}  // namespace blink::test
