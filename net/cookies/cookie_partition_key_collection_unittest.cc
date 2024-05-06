// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using testing::UnorderedElementsAre;

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
      CookiePartitionKeyCollection::FromOptional(std::nullopt);
  EXPECT_TRUE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());

  key_collection = CookiePartitionKeyCollection::FromOptional(
      std::make_optional<CookiePartitionKey>(
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"))));
  EXPECT_FALSE(key_collection.IsEmpty());
  EXPECT_FALSE(key_collection.ContainsAllKeys());
  EXPECT_THAT(key_collection.PartitionKeys(),
              UnorderedElementsAre(CookiePartitionKey::FromURLForTesting(
                  GURL("https://www.foo.com"))));
}

TEST(CookiePartitionKeyCollectionTest, Contains) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"));
  const CookiePartitionKey kOtherPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"));
  const CookiePartitionKey kPartitionKeyNotInCollection =
      CookiePartitionKey::FromURLForTesting(GURL("https://foobar.com"));

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
      {CookiePartitionKeyCollection({kPartitionKey, kOtherPartitionKey}),
       kPartitionKeyNotInCollection, false},
      // Contains all keys
      {CookiePartitionKeyCollection::ContainsAll(), kPartitionKey, true},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expects_contains,
              test_case.keychain.Contains(test_case.key));
  }
}

TEST(CookiePartitionKeyCollectionTest, Equals) {
  CookiePartitionKeyCollection empty;
  CookiePartitionKeyCollection foo(
      CookiePartitionKey::FromURLForTesting(GURL("https://foo.test")));
  CookiePartitionKeyCollection bar(
      CookiePartitionKey::FromURLForTesting(GURL("https://bar.test")));
  CookiePartitionKeyCollection all =
      CookiePartitionKeyCollection::ContainsAll();

  EXPECT_EQ(empty, empty);
  EXPECT_EQ(foo, foo);
  EXPECT_EQ(bar, bar);
  EXPECT_EQ(all, all);

  EXPECT_NE(foo, empty);
  EXPECT_NE(empty, foo);

  EXPECT_NE(foo, bar);
  EXPECT_NE(bar, foo);

  EXPECT_NE(foo, all);
  EXPECT_NE(all, foo);
}

class AncestorChainBitCookiePartitionKeyCollectionTest
    : public testing::TestWithParam<bool> {
 protected:
  // testing::Test
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kAncestorChainBitEnabledInPartitionedCookies,
        AncestorChainBitEnabled());
  }

  bool AncestorChainBitEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AncestorChainBitCookiePartitionKeyCollectionTest,
                         ::testing::Bool());

TEST_P(AncestorChainBitCookiePartitionKeyCollectionTest,
       ConsidersAncestorChainBit) {
  CookiePartitionKey cross_site_key = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.test"),
      CookiePartitionKey::AncestorChainBit::kCrossSite);

  CookiePartitionKey same_site_key = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.test"),
      CookiePartitionKey::AncestorChainBit::kSameSite);

  CookiePartitionKeyCollection cross_site_collection(cross_site_key);
  CookiePartitionKeyCollection same_site_collection(same_site_key);
  CookiePartitionKeyCollection all =
      CookiePartitionKeyCollection::ContainsAll();

  // Confirm that CookiePartitionKeyCollection::ContainsAll() is not impacted by
  // the value of the AncestorChainBit.
  EXPECT_TRUE(all.Contains(cross_site_key));
  EXPECT_TRUE(all.Contains(same_site_key));

  // Confirm that the results of equivalency and Contains() are
  // dependent on if the ancestor chain bit is enabled.
  if (AncestorChainBitEnabled()) {
    EXPECT_NE(cross_site_collection, same_site_collection);
    EXPECT_FALSE(cross_site_collection.Contains(same_site_key));
    EXPECT_FALSE(same_site_collection.Contains(cross_site_key));
  } else {
    EXPECT_EQ(cross_site_collection, same_site_collection);
    EXPECT_TRUE(cross_site_collection.Contains(same_site_key));
    EXPECT_TRUE(same_site_collection.Contains(cross_site_key));
  }
}
}  // namespace net
