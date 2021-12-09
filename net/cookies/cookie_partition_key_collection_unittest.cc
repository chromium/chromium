// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CookiePartitionKeyCollectionTest, EmptySet) {
  CookiePartitionKeyCollection key_collection;

  EXPECT_TRUE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_EQ(0u, key_collection.PartitionKeys().size());
}

TEST(CookiePartitionKeyCollectionTest, SingletonSet) {
  CookiePartitionKeyCollection key_collection(
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));

  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_THAT(
      key_collection.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeyCollectionTest, MultipleElements) {
  CookiePartitionKeyCollection key_collection({
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")),
  });

  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_THAT(
      key_collection.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(
              GURL("https://subdomain.foo.com")),
          CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"))));
}

TEST(CookiePartitionKeyCollectionTest, ContainsAll) {
  CookiePartitionKeyCollection key_collection =
      CookiePartitionKeyCollection::ContainsAll();
  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_TRUE(key_collection.ContainsAllKeys());
}

TEST(CookiePartitionKeyCollectionTest, FromOptional) {
  CookiePartitionKeyCollection key_collection =
      CookiePartitionKeyCollection::FromOptional(absl::nullopt);
  EXPECT_TRUE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());

  key_collection = CookiePartitionKeyCollection::FromOptional(
      absl::make_optional<CookiePartitionKey>(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_THAT(
      key_collection.PartitionKeys(),
      testing::UnorderedElementsAre(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeyCollectionTest, FirstPartySetify) {
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

  CookiePartitionKeyCollection empty_key_collection;
  EXPECT_TRUE(empty_key_collection.FirstPartySetify(&delegate).IsEmpty());
  EXPECT_TRUE(empty_key_collection.FirstPartySetify(nullptr).IsEmpty());

  CookiePartitionKeyCollection contains_all_keys =
      CookiePartitionKeyCollection::ContainsAll();
  EXPECT_TRUE(contains_all_keys.FirstPartySetify(&delegate).ContainsAllKeys());
  EXPECT_TRUE(contains_all_keys.FirstPartySetify(nullptr).ContainsAllKeys());

  // An owner site of an FPS should not have its partition key changed.
  CookiePartitionKeyCollection got =
      CookiePartitionKeyCollection(kOwnerPartitionKey)
          .FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);

  // A member site should have its partition key changed to the owner site.
  got = CookiePartitionKeyCollection(kMemberPartitionKey)
            .FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);

  // A member site's partition key should not change if the CookieAccessDelegate
  // is null.
  got = CookiePartitionKeyCollection(kMemberPartitionKey)
            .FirstPartySetify(nullptr);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kMemberPartitionKey, got.PartitionKeys()[0]);

  // A non-member site should not have its partition key changed.
  got = CookiePartitionKeyCollection(kNonMemberPartitionKey)
            .FirstPartySetify(&delegate);
  EXPECT_EQ(1u, got.PartitionKeys().size());
  EXPECT_EQ(kNonMemberPartitionKey, got.PartitionKeys()[0]);

  // A key collection that contains a member site and non-member site should be
  // changed to include the owner site and the unmodified non-member site.
  got = CookiePartitionKeyCollection(
            {kMemberPartitionKey, kNonMemberPartitionKey})
            .FirstPartySetify(&delegate);
  EXPECT_EQ(2u, got.PartitionKeys().size());
  EXPECT_EQ(kOwnerPartitionKey, got.PartitionKeys()[0]);
  EXPECT_EQ(kNonMemberPartitionKey, got.PartitionKeys()[1]);
}

}  // namespace net
