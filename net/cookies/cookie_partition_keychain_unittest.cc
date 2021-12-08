// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_
#define NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_

#include "net/cookies/cookie_partition_keychain.h"
#include "net/cookies/test_cookie_access_delegate.h"
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

TEST(CookiePartitionKeychainTest, FirstPartySetify) {
  const GURL kOwnerURL("https://owner.com");
  const SchemefulSite kOwnerSite(kOwnerURL);
  const CookiePartitionKey kOwnerPartitionKey =
      CookiePartitionKey::FromURLForTesting(kOwnerURL);

  const GURL kMemberURL("https://member.com");
  const SchemefulSite kMemberSite(kMemberURL);
  const CookiePartitionKey kMemberPartitionKey =
      CookiePartitionKey::FromURLForTesting(kMemberURL);

  const GURL kNonMemberURL("https://nonmember.com");
  const CookiePartitionKey kNonMemberPartitionKey =
      CookiePartitionKey::FromURLForTesting(kNonMemberURL);

  TestCookieAccessDelegate delegate;
  base::flat_map<SchemefulSite, std::set<SchemefulSite>> first_party_sets;
  first_party_sets.insert(std::make_pair(
      kOwnerSite, std::set<SchemefulSite>({kOwnerSite, kMemberSite})));
  delegate.SetFirstPartySets(first_party_sets);

  CookiePartitionKeychain empty_keychain;
  EXPECT_TRUE(empty_keychain.FirstPartySetify(&delegate).IsEmpty());
  EXPECT_TRUE(empty_keychain.FirstPartySetify(nullptr).IsEmpty());

  CookiePartitionKeychain contains_all_keys =
      CookiePartitionKeychain::ContainsAll();
  EXPECT_TRUE(contains_all_keys.FirstPartySetify(&delegate).ContainsAllKeys());
  EXPECT_TRUE(contains_all_keys.FirstPartySetify(nullptr).ContainsAllKeys());

  // An owner site of an FPS should not have its partition key changed.
  CookiePartitionKeychain got =
      CookiePartitionKeychain(kOwnerPartitionKey).FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);

  // A member site should have its partition key changed to the owner site.
  got =
      CookiePartitionKeychain(kMemberPartitionKey).FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);

  // A member site's partition key should not change if the CookieAccessDelegate
  // is null.
  got = CookiePartitionKeychain(kMemberPartitionKey).FirstPartySetify(nullptr);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kMemberPartitionKey, got.PartitionKeys()[0]);

  // A non-member site should not have its partition key changed.
  got = CookiePartitionKeychain(kNonMemberPartitionKey)
            .FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kNonMemberPartitionKey, got.PartitionKeys()[0]);

  // A keychain that contains a member site and non-member site should be
  // changed to include the owner site and the unmodified non-member site.
  got = CookiePartitionKeychain({kMemberPartitionKey, kNonMemberPartitionKey})
            .FirstPartySetify(&delegate);
  EXPECT_EQ(2u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);
  EXPECT_EQ(kNonMemberPartitionKey, got.PartitionKeys()[1]);
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_UNITTEST_H_
