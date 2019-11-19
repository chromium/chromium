// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/esni_content.h"

#include "base/strings/string_number_conversions.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

IPAddress MakeIPAddress() {
  // Introduce some (deterministic) variation in the IP addresses
  // generated.
  static uint8_t next_octet = 0;
  next_octet += 4;

  return IPAddress(next_octet, next_octet + 1, next_octet + 2, next_octet + 3);
}

// Make sure we can add keys.
TEST(EsniContentTest, AddKey) {
  EsniContent c1;
  c1.AddKey("a");
  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a"));
  c1.AddKey("a");
  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a"));
  c1.AddKey("b");
  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a", "b"));
}

// Make sure we can add key-address pairs.
TEST(EsniContentTest, AddKeyForAddress) {
  EsniContent c1;
  auto address = MakeIPAddress();
  c1.AddKeyForAddress(address, "a");
  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a"));
  EXPECT_THAT(c1.keys_for_addresses(),
              ::testing::UnorderedElementsAre(::testing::Pair(
                  address, ::testing::UnorderedElementsAre("a"))));
}

TEST(EsniContentTest, AssociateAddressWithExistingKey) {
  EsniContent c1;
  auto address = MakeIPAddress();
  c1.AddKey("a");
  c1.AddKeyForAddress(address, "a");
  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a"));
  EXPECT_THAT(c1.keys_for_addresses(),
              ::testing::UnorderedElementsAre(::testing::Pair(
                  address, ::testing::UnorderedElementsAre("a"))));
}

// Merging to an empty EsniContent should make the result equal the source of
// the merge.
TEST(EsniContentTest, MergeToEmpty) {
  EsniContent c1;
  c1.AddKey("c");
  IPAddress address = MakeIPAddress();

  c1.AddKeyForAddress(address, "a");
  c1.AddKeyForAddress(address, "b");
  EsniContent empty;
  empty.MergeFrom(c1);
  EXPECT_EQ(c1, empty);
}

TEST(EsniContentTest, MergeFromEmptyNoOp) {
  EsniContent c1, c2;
  c1.AddKey("a");
  c2.AddKey("a");
  EsniContent empty;
  c1.MergeFrom(empty);
  EXPECT_EQ(c1, c2);
}

// Test that merging multiple keys corresponding to a single address works.
TEST(EsniContentTest, MergeKeysForSingleHost) {
  EsniContent c1, c2;
  IPAddress address = MakeIPAddress();

  c1.AddKeyForAddress(address, "a");
  c1.AddKeyForAddress(address, "b");
  c2.AddKeyForAddress(address, "b");
  c2.AddKeyForAddress(address, "c");
  c1.MergeFrom(c2);

  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a", "b", "c"));
  EXPECT_THAT(c1.keys_for_addresses(),
              ::testing::UnorderedElementsAre(::testing::Pair(
                  address, ::testing::UnorderedElementsAre("a", "b", "c"))));
}

// Test that merging multiple addresss corresponding to a single key works.
TEST(EsniContentTest, MergeHostsForSingleKey) {
  EsniContent c1, c2;
  IPAddress address = MakeIPAddress();
  IPAddress second_address = MakeIPAddress();
  c1.AddKeyForAddress(address, "a");
  c2.AddKeyForAddress(second_address, "a");
  c1.MergeFrom(c2);

  EXPECT_THAT(c1.keys(), ::testing::UnorderedElementsAre("a"));
  EXPECT_THAT(
      c1.keys_for_addresses(),
      ::testing::UnorderedElementsAre(
          ::testing::Pair(address, ::testing::UnorderedElementsAre("a")),
          ::testing::Pair(second_address,
                          ::testing::UnorderedElementsAre("a"))));
}

// Test merging some more complex instances of the class.
TEST(EsniContentTest, MergeSeveralHostsAndKeys) {
  EsniContent c1, c2, expected;
  for (int i = 0; i < 50; ++i) {
    IPAddress address = MakeIPAddress();
    std::string key = base::NumberToString(i);
    switch (i % 3) {
      case 0:
        c1.AddKey(key);
        expected.AddKey(key);
        break;
      case 1:
        c2.AddKey(key);
        expected.AddKey(key);
        break;
    }
    // Associate each address with a subset of the keys seen so far
    {
      int j = 0;
      for (auto key : c1.keys()) {
        if (j % 2) {
          c1.AddKeyForAddress(address, key);
          expected.AddKeyForAddress(address, key);
        }
        ++j;
      }
    }
    {
      int j = 0;
      for (auto key : c2.keys()) {
        if (j % 3 == 1) {
          c2.AddKeyForAddress(address, key);
          expected.AddKeyForAddress(address, key);
        }
        ++j;
      }
    }
  }
  {
    EsniContent merge_dest = c1;
    EsniContent merge_src = c2;
    merge_dest.MergeFrom(merge_src);
    EXPECT_EQ(merge_dest, expected);
  }
  {
    EsniContent merge_dest = c2;
    EsniContent merge_src = c1;
    merge_dest.MergeFrom(merge_src);
    EXPECT_EQ(merge_dest, expected);
  }
}

}  // namespace

}  // namespace net
