// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_host_resolver_cache.h"

#include "base/test/task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proxy_resolver {
namespace {

class ProxyHostResolverCacheTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProxyHostResolverCache cache_;
};

TEST_F(ProxyHostResolverCacheTest, SimpleNegativeLookup) {
  ASSERT_EQ(cache_.GetSizeForTesting(), 0u);
  EXPECT_FALSE(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                  /*is_ex_operation=*/false));
}

TEST_F(ProxyHostResolverCacheTest, SimpleCachedLookup) {
  const net::IPAddress kResult(1, 2, 3, 4);

  cache_.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                    /*is_ex_operation=*/false, {kResult});

  EXPECT_EQ(cache_.GetSizeForTesting(), 1u);
  EXPECT_THAT(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false),
              testing::Pointee(testing::ElementsAre(kResult)));
}

TEST_F(ProxyHostResolverCacheTest, NoResultWithNonMatchingKeyFields) {
  cache_.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                    /*is_ex_operation=*/false, {net::IPAddress(1, 2, 3, 5)});
  ASSERT_EQ(cache_.GetSizeForTesting(), 1u);

  // Non-matching hostname
  EXPECT_FALSE(cache_.LookupEntry("host1.test", net::NetworkAnonymizationKey(),
                                  /*is_ex_operation=*/false));

  // Non-matching anonymization key
  EXPECT_FALSE(cache_.LookupEntry(
      "host.test", net::NetworkAnonymizationKey::CreateTransient(),
      /*is_ex_operation=*/false));

  // Non-matching `is_ex_operation`
  EXPECT_FALSE(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                  /*is_ex_operation=*/true));
}

TEST_F(ProxyHostResolverCacheTest, NoResultForExpiredLookup) {
  const net::IPAddress kResult(1, 2, 3, 6);

  cache_.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                    /*is_ex_operation=*/false, {kResult});

  task_environment_.FastForwardBy(ProxyHostResolverCache::kTtl -
                                  base::Milliseconds(5));
  EXPECT_EQ(cache_.GetSizeForTesting(), 1u);
  ASSERT_THAT(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false),
              testing::Pointee(testing::ElementsAre(kResult)));

  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_FALSE(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                  /*is_ex_operation=*/false));

  // Expect expired entry to be deleted by lookup attempt.
  EXPECT_EQ(cache_.GetSizeForTesting(), 0u);
}

TEST_F(ProxyHostResolverCacheTest, EvictsOldestEntriesWhenFull) {
  ProxyHostResolverCache cache(/*max_entries=*/3u);

  // Initial entry to be deleted.
  cache.StoreEntry("to-be-deleted.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});

  // Fill to max capacity
  task_environment_.FastForwardBy(base::Milliseconds(5));
  cache.StoreEntry("other1.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  cache.StoreEntry("other2.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});

  // Nothing should be evicted yet.
  EXPECT_EQ(cache.GetSizeForTesting(), 3u);
  EXPECT_TRUE(cache.LookupEntry("to-be-deleted.test",
                                net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("other1.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("other2.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));

  // Add another entry and expect eviction of oldest.
  cache.StoreEntry("evictor.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});

  EXPECT_EQ(cache.GetSizeForTesting(), 3u);
  EXPECT_FALSE(cache.LookupEntry("to-be-deleted.test",
                                 net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("other1.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("other2.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("evictor.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
}

TEST_F(ProxyHostResolverCacheTest, UpdatesAlreadyExistingEntryWithSameKey) {
  cache_.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                    /*is_ex_operation=*/false, /*results=*/{});
  ASSERT_EQ(cache_.GetSizeForTesting(), 1u);

  const net::IPAddress kResult(1, 2, 3, 7);
  cache_.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                    /*is_ex_operation=*/false, {kResult});

  EXPECT_EQ(cache_.GetSizeForTesting(), 1u);
  EXPECT_THAT(cache_.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false),
              testing::Pointee(testing::ElementsAre(kResult)));
}

TEST_F(ProxyHostResolverCacheTest, EntryUpdateRefreshesExpiration) {
  ProxyHostResolverCache cache(/*max_entries=*/2u);

  // Insert two entries, with "to-be-refreshed.test" as the older one.
  cache.StoreEntry("to-be-refreshed.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  task_environment_.FastForwardBy(base::Milliseconds(5));
  cache.StoreEntry("to-be-evicted.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  ASSERT_EQ(cache.GetSizeForTesting(), 2u);

  // Update "to-be-refreshed.test" to refresh its expiration.
  task_environment_.FastForwardBy(base::Milliseconds(5));
  cache.StoreEntry("to-be-refreshed.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  ASSERT_EQ(cache.GetSizeForTesting(), 2u);

  // Add another entry to force an eviction.
  cache.StoreEntry("evictor.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});

  EXPECT_EQ(cache.GetSizeForTesting(), 2u);
  EXPECT_FALSE(cache.LookupEntry("to-be-evicted.test",
                                 net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("to-be-refreshed.test",
                                net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("evictor.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
}

TEST_F(ProxyHostResolverCacheTest, EntryCanBeEvictedAfterUpdate) {
  ProxyHostResolverCache cache(/*max_entries=*/1u);

  // Add entry and then update it.
  cache.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  ASSERT_EQ(cache.GetSizeForTesting(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(5));
  cache.StoreEntry("host.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});
  ASSERT_EQ(cache.GetSizeForTesting(), 1u);

  // Add another entry to force an eviction.
  task_environment_.FastForwardBy(base::Milliseconds(5));
  cache.StoreEntry("evictor.test", net::NetworkAnonymizationKey(),
                   /*is_ex_operation=*/false, /*results=*/{});

  EXPECT_EQ(cache.GetSizeForTesting(), 1u);
  EXPECT_FALSE(cache.LookupEntry("host.test", net::NetworkAnonymizationKey(),
                                 /*is_ex_operation=*/false));
  EXPECT_TRUE(cache.LookupEntry("evictor.test", net::NetworkAnonymizationKey(),
                                /*is_ex_operation=*/false));
}

}  // namespace
}  // namespace proxy_resolver
