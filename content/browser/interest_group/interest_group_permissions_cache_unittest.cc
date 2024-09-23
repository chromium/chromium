// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_permissions_cache.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Very short time used by some tests that want to wait until just after a
// timer triggers.
constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

using Permissions = InterestGroupPermissionsCache::Permissions;

class InterestGroupPermissionsCacheTest : public testing::Test {
 protected:
  // Default values used in most tests.
  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://frame.test"));
  const url::Origin kGroupOrigin =
      url::Origin::Create(GURL("https://group.test"));
  const net::SchemefulSite kFrameSite = net::SchemefulSite(kFrameOrigin);
  const net::NetworkIsolationKey kNetworkIsolationKey =
      net::NetworkIsolationKey(kFrameSite, kFrameSite);
  const Permissions kPermissions =
      Permissions{/*can_join=*/true, /*can_leave=*/false};

  // Alternative values when two different values are needed for any field.
  const url::Origin kOtherFrameOrigin =
      url::Origin::Create(GURL("https://other_frame.test"));
  const url::Origin kOtherGroupOrigin =
      url::Origin::Create(GURL("https://other_group.test"));
  const net::SchemefulSite kOtherFrameSite =
      net::SchemefulSite(kOtherFrameOrigin);
  const net::NetworkIsolationKey kOtherNetworkIsolationKey =
      net::NetworkIsolationKey(kOtherFrameSite, kOtherFrameSite);
  const Permissions kOtherPermissions =
      Permissions{/*can_join=*/false, /*can_leave=*/true};

  base::test::TaskEnvironment task_environment_ = base::test::TaskEnvironment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  InterestGroupPermissionsCache interest_group_permissions_cache_;
};

TEST_F(InterestGroupPermissionsCacheTest, PermissionsEquality) {
  EXPECT_EQ(kPermissions, kPermissions);
  EXPECT_EQ(kOtherPermissions, kOtherPermissions);

  EXPECT_NE(kOtherPermissions, kPermissions);
  EXPECT_NE(kPermissions, kOtherPermissions);

  // Check that two different Permissions objects that have the same values are
  // considered equal.
  //
  // Same as kPermissions.
  Permissions permissions1{/*can_join=*/true, /*can_leave=*/false};
  EXPECT_EQ(kPermissions, permissions1);
  EXPECT_EQ(permissions1, kPermissions);

  // Matches neither `kPermissions` nor `kOtherPermissions`. Used to make sure
  // both values matter.
  Permissions permissions2{/*can_join=*/false, /*can_leave=*/false};
  EXPECT_NE(kPermissions, permissions2);
  EXPECT_NE(permissions2, kPermissions);
  EXPECT_NE(kOtherPermissions, permissions2);
  EXPECT_NE(permissions2, kOtherPermissions);
}

TEST_F(InterestGroupPermissionsCacheTest, Basic) {
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));

  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);
}

TEST_F(InterestGroupPermissionsCacheTest, Overwrite) {
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);

  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);
}

TEST_F(InterestGroupPermissionsCacheTest, MultipleEntries) {
  // Permissions that are each assigned to a different pair of origins +
  // NetworkIsolationKey. This coincidentally covers all distinct permissions
  // values, but that is not necessary for this test. They all just need to be
  // distinct.
  const Permissions kPermissionsValues[4] = {
      {/*can_join=*/true, /*can_leave=*/true},
      {/*can_join=*/true, /*can_leave=*/false},
      {/*can_join=*/false, /*can_leave=*/true},
      {/*can_join=*/false, /*can_leave=*/false},
  };

  // Each set of permissions varies in only one value from the first set. Some
  // of these combinations can't actually occur (in particular, the frame origin
  // normally corresponds to the NetworkIsolationKey's frame site), but the
  // cache layer doesn't care.
  interest_group_permissions_cache_.CachePermissions(
      kPermissionsValues[0], kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  interest_group_permissions_cache_.CachePermissions(
      kPermissionsValues[1], kOtherFrameOrigin, kGroupOrigin,
      kNetworkIsolationKey);
  interest_group_permissions_cache_.CachePermissions(
      kPermissionsValues[2], kFrameOrigin, kOtherGroupOrigin,
      kNetworkIsolationKey);
  interest_group_permissions_cache_.CachePermissions(kPermissionsValues[3],
                                                     kFrameOrigin, kGroupOrigin,
                                                     kOtherNetworkIsolationKey);

  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissionsValues[0]);

  permissions = interest_group_permissions_cache_.GetPermissions(
      kOtherFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissionsValues[1]);

  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissionsValues[2]);

  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kOtherNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissionsValues[3]);
}

TEST_F(InterestGroupPermissionsCacheTest, Clear) {
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);

  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kOtherFrameOrigin, kOtherGroupOrigin,
      kOtherNetworkIsolationKey);
  permissions = interest_group_permissions_cache_.GetPermissions(
      kOtherFrameOrigin, kOtherGroupOrigin, kOtherNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);
  EXPECT_EQ(2u, interest_group_permissions_cache_.cache_shards_for_testing());

  interest_group_permissions_cache_.Clear();
  EXPECT_EQ(0u, interest_group_permissions_cache_.cache_shards_for_testing());

  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kOtherFrameOrigin, kOtherGroupOrigin, kOtherNetworkIsolationKey));
}

TEST_F(InterestGroupPermissionsCacheTest, Expiry) {
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kCacheDuration);
  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  task_environment_.FastForwardBy(kTinyTime);
  // Cache shards are only deleted on a timer or on access, neither of which has
  // happened yet, though the shard should now be expired.
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));
  // The expired cache shard should have been deleted on access.
  EXPECT_EQ(0u, interest_group_permissions_cache_.cache_shards_for_testing());
}

// Test the case where a CacheEntry expires without the cache shard also
// expiring.
TEST_F(InterestGroupPermissionsCacheTest, CacheEntryExpiry) {
  // Create a cache entry.
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Wait until just before the entry expires.
  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kCacheDuration);

  // Add another entry to the same cache shard.
  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);

  // There should still be only one cache shard.
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Check that the original entry is still there.
  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Wait for the origin entry to expire.
  task_environment_.FastForwardBy(kTinyTime);

  // Original entry should have expired.
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));

  // Other entry should still be present.
  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);

  // There should still be only one cache shard.
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());
}

TEST_F(InterestGroupPermissionsCacheTest, DeleteExpired) {
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);

  // Run until just before the delete expired timer runs. Don't call
  // GetPermissions(), as it would delete the entry. At this point, the entry is
  // expired, but has not been deleted.
  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kDeleteExpiredTimerDuration - kTinyTime);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Run until the delete expired timer runs, which should delete the entry.
  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(0u, interest_group_permissions_cache_.cache_shards_for_testing());
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));
}

TEST_F(InterestGroupPermissionsCacheTest, DeleteExpiredPreservesUnexpired) {
  interest_group_permissions_cache_.CachePermissions(
      kPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Run until just before the delete expired timer runs. Don't call
  // GetPermissions(), as it would delete the entry. At this point, the entry is
  // expired, but has not been deleted.
  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kDeleteExpiredTimerDuration - kTinyTime);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Add permission for a different group.
  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Run until the delete expired timer runs, which should delete nothing.
  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // The original permissions should have expired.
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));

  // But the new permissions should still be around.
  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);
  EXPECT_EQ(1u, interest_group_permissions_cache_.cache_shards_for_testing());

  // Run until the delete expired timer triggers again, which should now delete
  // the entry.
  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kDeleteExpiredTimerDuration);
  EXPECT_EQ(0u, interest_group_permissions_cache_.cache_shards_for_testing());
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey));
}

TEST_F(InterestGroupPermissionsCacheTest, LRU) {
  // Fill LRU cache.
  std::vector<url::Origin> group_origins;
  for (int i = 0; i < InterestGroupPermissionsCache::kMaxCacheEntriesPerShard;
       ++i) {
    url::Origin group_origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i)));
    interest_group_permissions_cache_.CachePermissions(
        kPermissions, kFrameOrigin, group_origin, kNetworkIsolationKey);
    group_origins.emplace_back(std::move(group_origin));
  }

  // Check all entries are present, accessing in order to end with the same LRU
  // order as before.
  for (int i = 0; i < InterestGroupPermissionsCache::kMaxCacheEntriesPerShard;
       ++i) {
    Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
        kFrameOrigin, group_origins[i], kNetworkIsolationKey);
    ASSERT_TRUE(permissions);
    EXPECT_EQ(*permissions, kPermissions);
  }

  // Access first entry.
  ASSERT_TRUE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, group_origins[0], kNetworkIsolationKey));

  // Overwrite second entry.
  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kFrameOrigin, group_origins[1], kNetworkIsolationKey);

  // Add another entry. The third entry is the last-used entry, and should be
  // evicted.
  interest_group_permissions_cache_.CachePermissions(
      kOtherPermissions, kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);

  // Check third entry was removed.
  EXPECT_FALSE(interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, group_origins[2], kNetworkIsolationKey));

  // Check other entries are all still present.

  for (int i = 3; i < InterestGroupPermissionsCache::kMaxCacheEntriesPerShard;
       ++i) {
    Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
        kFrameOrigin, group_origins[i], kNetworkIsolationKey);
    ASSERT_TRUE(permissions);
    EXPECT_EQ(*permissions, kPermissions);
  }

  Permissions* permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, group_origins[0], kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kPermissions);

  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, group_origins[1], kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);

  permissions = interest_group_permissions_cache_.GetPermissions(
      kFrameOrigin, kGroupOrigin, kNetworkIsolationKey);
  ASSERT_TRUE(permissions);
  EXPECT_EQ(*permissions, kOtherPermissions);
}

}  // namespace
}  // namespace content
