// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_
#define NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_

#include "net/cookies/cookie_partition_keychain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CookiePartitionKeychainTest, EmptySet) {
  CookiePartitionKeychain keychain;

  EXPECT_TRUE(keychain.IsEmpty());
  EXPECT_FALSE(keychain.ContainsAllKeys());
  EXPECT_EQ(0u, keychain.PartitionKeys().size());
}

TEST(CookiePartitionKeychainTest, SingletonSet) {
  CookiePartitionKeychain keychain(
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));

  EXPECT_FALSE(keychain.IsEmpty());
  EXPECT_FALSE(keychain.ContainsAllKeys());
  EXPECT_THAT(
      keychain.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeychainTest, MultipleElements) {
  CookiePartitionKeychain keychain({
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")),
  });

  EXPECT_FALSE(keychain.IsEmpty());
  EXPECT_FALSE(keychain.ContainsAllKeys());
  EXPECT_THAT(
      keychain.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(
              GURL("https://subdomain.foo.com")),
          CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"))));
}

TEST(CookiePartitionKeychainTest, ContainsAll) {
  CookiePartitionKeychain keychain = CookiePartitionKeychain::ContainsAll();
  EXPECT_FALSE(keychain.IsEmpty());
  EXPECT_TRUE(keychain.ContainsAllKeys());
}

TEST(CookiePartitionKeychainTest, FromOptional) {
  CookiePartitionKeychain keychain =
      CookiePartitionKeychain::FromOptional(absl::nullopt);
  EXPECT_TRUE(keychain.IsEmpty());
  EXPECT_FALSE(keychain.ContainsAllKeys());

  keychain = CookiePartitionKeychain::FromOptional(
      absl::make_optional<CookiePartitionKey>(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
  EXPECT_FALSE(keychain.IsEmpty());
  EXPECT_FALSE(keychain.ContainsAllKeys());
  EXPECT_THAT(
      keychain.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_
