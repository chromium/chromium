// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/ip_address.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(IPAddressStructTraitsTest, Ipv4) {
  IPAddress original(1, 2, 3, 4);

  IPAddress deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::IPAddress>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(IPAddressStructTraitsTest, Ipv6) {
  IPAddress original(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

  IPAddress deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::IPAddress>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

// Serialization/deserialization not expected to work for invalid addresses,
// e.g. an address with 5 octets.
TEST(IPAddressStructTraitsTest, InvalidAddress) {
  uint8_t bad_address_bytes[] = {1, 2, 3, 4, 5};
  IPAddress original(bad_address_bytes);
  ASSERT_FALSE(original.IsValid());

  IPAddress deserialized;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<network::mojom::IPAddress>(
      original, deserialized));
}

}  // namespace
}  // namespace net
