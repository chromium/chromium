// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using testing::UnorderedElementsAre;

namespace {

// Synchronous wrapper around CookiePartitionKeyCollection::FirstPartySetify.
// Spins the event loop.
CookiePartitionKeyCollection FirstPartySetifyAndWait(
    const CookiePartitionKeyCollection& collection,
    const CookieAccessDelegate* cookie_access_delegate) {
  base::RunLoop run_loop;
  CookiePartitionKeyCollection canonicalized_collection;
  absl::optional<CookiePartitionKeyCollection> maybe_collection =
      collection.FirstPartySetify(
          cookie_access_delegate,
          base::BindLambdaForTesting([&](CookiePartitionKeyCollection result) {
            canonicalized_collection = result;
            run_loop.Quit();
          }));
  if (maybe_collection.has_value())
    return maybe_collection.value();
  run_loop.Run();
  return canonicalized_collection;
}

}  // namespace

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
  EXPECT_THAT(key_collection.PartitionKeys(),
              UnorderedElementsAre(CookiePartitionKey::FromURLForTesting(
                  GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeyCollectionTest, MultipleElements) {
  CookiePartitionKeyCollection key_collection({
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")),
  });

  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_THAT(key_collection.PartitionKeys(),
              UnorderedElementsAre(CookiePartitionKey::FromURLForTesting(
                                       GURL("https://subdomain.foo.com")),
                                   CookiePartitionKey::FromURLForTesting(
                                       GURL("https://www.bar.com"))));
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
  EXPECT_THAT(key_collection.PartitionKeys(),
              UnorderedElementsAre(CookiePartitionKey::FromURLForTesting(
                  GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeyCollectionTest, FirstPartySetify) {
  base::test::TaskEnvironment env;
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
  delegate.SetFirstPartySets({
      {kOwnerSite, net::FirstPartySetEntry(kOwnerSite, net::SiteType::kPrimary,
                                           absl::nullopt)},
      {kMemberSite,
       net::FirstPartySetEntry(kOwnerSite, net::SiteType::kAssociated, 0)},
  });

  CookiePartitionKeyCollection empty_key_collection;
  EXPECT_TRUE(
      FirstPartySetifyAndWait(empty_key_collection, &delegate).IsEmpty());
  EXPECT_TRUE(FirstPartySetifyAndWait(empty_key_collection, nullptr).IsEmpty());

  CookiePartitionKeyCollection contains_all_keys =
      CookiePartitionKeyCollection::ContainsAll();
  EXPECT_TRUE(
      FirstPartySetifyAndWait(contains_all_keys, &delegate).ContainsAllKeys());
  EXPECT_TRUE(
      FirstPartySetifyAndWait(contains_all_keys, nullptr).ContainsAllKeys());

  // An owner site of an FPS should not have its partition key changed.
  EXPECT_THAT(FirstPartySetifyAndWait(
                  CookiePartitionKeyCollection(kOwnerPartitionKey), &delegate)
                  .PartitionKeys(),
              UnorderedElementsAre(kOwnerPartitionKey));

  // A member site should have its partition key changed to the owner site.
  EXPECT_THAT(FirstPartySetifyAndWait(
                  CookiePartitionKeyCollection(kMemberPartitionKey), &delegate)
                  .PartitionKeys(),
              UnorderedElementsAre(kOwnerPartitionKey));

  // A member site's partition key should not change if the CookieAccessDelegate
  // is null.
  EXPECT_THAT(FirstPartySetifyAndWait(
                  CookiePartitionKeyCollection(kMemberPartitionKey), nullptr)
                  .PartitionKeys(),
              UnorderedElementsAre(kMemberPartitionKey));

  // A non-member site should not have its partition key changed.
  EXPECT_THAT(
      FirstPartySetifyAndWait(
          CookiePartitionKeyCollection(kNonMemberPartitionKey), &delegate)
          .PartitionKeys(),
      UnorderedElementsAre(kNonMemberPartitionKey));

  // A key collection that contains a member site and non-member site should be
  // changed to include the owner site and the unmodified non-member site.
  EXPECT_THAT(FirstPartySetifyAndWait(
                  CookiePartitionKeyCollection(
                      {kMemberPartitionKey, kNonMemberPartitionKey}),
                  &delegate)
                  .PartitionKeys(),
              UnorderedElementsAre(kOwnerPartitionKey, kNonMemberPartitionKey));

  // Test that FirstPartySetify does not modify partition keys with nonces.
  const CookiePartitionKey kNoncedPartitionKey =
      CookiePartitionKey::FromURLForTesting(kMemberURL,
                                            base::UnguessableToken::Create());
  EXPECT_THAT(
      FirstPartySetifyAndWait(
          CookiePartitionKeyCollection({kNoncedPartitionKey}), &delegate)
          .PartitionKeys(),
      UnorderedElementsAre(kNoncedPartitionKey));
  EXPECT_THAT(
      FirstPartySetifyAndWait(CookiePartitionKeyCollection(
                                  {kNoncedPartitionKey, kMemberPartitionKey}),
                              &delegate)
          .PartitionKeys(),
      UnorderedElementsAre(kNoncedPartitionKey, kOwnerPartitionKey));
}

TEST(CookiePartitionKeyCollectionTest, Contains) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"));
  const CookiePartitionKey kOtherPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"));

  struct TestCase {
    const CookiePartitionKeyCollection keychain;
    const CookiePartitionKey key;
    bool expects_contains;
  } test_cases[] = {
      // Empty keychain
      {CookiePartitionKeyCollection(), kPartitionKey, false},
      // Singleton keychain with key
      {CookiePartitionKeyCollection(kPartitionKey), kPartitionKey, true},
      // Singleton keychain with different key
      {CookiePartitionKeyCollection(kOtherPartitionKey), kPartitionKey, false},
      // Multiple keys
      {CookiePartitionKeyCollection({kPartitionKey, kOtherPartitionKey}),
       kPartitionKey, true},
      // Contains all keys
      {CookiePartitionKeyCollection::ContainsAll(), kPartitionKey, true},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expects_contains,
              test_case.keychain.Contains(test_case.key));
  }
}

}  // namespace net
