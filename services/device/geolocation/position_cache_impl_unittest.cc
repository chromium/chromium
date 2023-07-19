// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/position_cache_impl.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/position_cache_test_util.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PositionCacheImplTest : public ::testing::Test {
 public:
  PositionCacheImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        cache_(task_environment_.GetMockTickClock()) {}

  mojom::PositionCacheDiagnosticsPtr GetDiagnostics() {
    auto diagnostics = mojom::PositionCacheDiagnostics::New();
    cache_.FillDiagnostics(*diagnostics);
    return diagnostics;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  PositionCacheImpl cache_;
};

TEST_F(PositionCacheImplTest, EmptyCacheReturnsNoLocations) {
  WifiData empty_wifi_data;
  EXPECT_EQ(nullptr, cache_.FindPosition(empty_wifi_data));
  EXPECT_EQ(0U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, CanAddEmptyWifiData) {
  WifiData empty_wifi_data;
  mojom::GeopositionPtr geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(empty_wifi_data, *geoposition);

  const mojom::Geoposition* found_position =
      cache_.FindPosition(empty_wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(geoposition->Equals(*found_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, FirstAddedWifiDataReturned) {
  WifiData wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(wifi_data, *geoposition);

  const mojom::Geoposition* found_position = cache_.FindPosition(wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(geoposition->Equals(*found_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, LastAddedWifiDataReturned) {
  mojom::GeopositionPtr first_geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(testing::CreateDefaultUniqueWifiData(),
                       *first_geoposition);
  mojom::GeopositionPtr second_geoposition = testing::CreateGeoposition(2);
  cache_.CachePosition(testing::CreateDefaultUniqueWifiData(),
                       *second_geoposition);

  WifiData final_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr final_geoposition = testing::CreateGeoposition(5);
  cache_.CachePosition(final_wifi_data, *final_geoposition);

  const mojom::Geoposition* found_position =
      cache_.FindPosition(final_wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(final_geoposition->Equals(*found_position));
  EXPECT_EQ(3U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, MaxPositionsFound) {
  std::vector<std::pair<WifiData, mojom::GeopositionPtr>> test_data;
  for (size_t i = 0; i < PositionCacheImpl::kMaximumSize; ++i) {
    test_data.push_back(std::make_pair(testing::CreateDefaultUniqueWifiData(),
                                       testing::CreateGeoposition(i)));
  }

  // Populate the cache
  for (const auto& test_data_pair : test_data) {
    cache_.CachePosition(test_data_pair.first, *test_data_pair.second);
  }
  EXPECT_EQ(PositionCacheImpl::kMaximumSize, cache_.GetPositionCacheSize());
  // Make sure all elements are cached.
  for (const auto& test_data_pair : test_data) {
    const mojom::Geoposition* found_position =
        cache_.FindPosition(test_data_pair.first);
    ASSERT_NE(nullptr, found_position);
    EXPECT_TRUE(test_data_pair.second->Equals(*found_position));
  }
}

TEST_F(PositionCacheImplTest, Eviction) {
  WifiData initial_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr initial_geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(initial_wifi_data, *initial_geoposition);
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());

  // Add as many entries as the cache's size limit, which should evict
  // |initial_wifi_data|.
  for (size_t i = 0; i < PositionCacheImpl::kMaximumSize; ++i) {
    cache_.CachePosition(testing::CreateDefaultUniqueWifiData(),
                         *testing::CreateGeoposition(i));
  }
  EXPECT_EQ(PositionCacheImpl::kMaximumSize, cache_.GetPositionCacheSize());

  // |initial_wifi_data| can no longer be found in cache_.
  ASSERT_EQ(nullptr, cache_.FindPosition(initial_wifi_data));
}

TEST_F(PositionCacheImplTest, LastUsedPositionRemembered) {
  // Initially, no last used position.
  EXPECT_FALSE(cache_.GetLastUsedNetworkPosition());
  // Remembered after setting.
  auto result = device::mojom::GeopositionResult::NewPosition(
      testing::CreateGeoposition(4));
  cache_.SetLastUsedNetworkPosition(*result);
  const auto* last_used_result = cache_.GetLastUsedNetworkPosition();
  ASSERT_TRUE(last_used_result);
  EXPECT_TRUE(result->Equals(*last_used_result));
}

TEST_F(PositionCacheImplTest, EntryEvictedAfterMaxLifetimeReached) {
  WifiData initial_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr initial_geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(initial_wifi_data, *initial_geoposition);

  // Initially, the position is there.
  const mojom::Geoposition* found_position =
      cache_.FindPosition(initial_wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(initial_geoposition->Equals(*found_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());

  task_environment_.FastForwardBy(PositionCacheImpl::kMaximumLifetime);

  // Position was evicted.
  EXPECT_EQ(nullptr, cache_.FindPosition(initial_wifi_data));
  EXPECT_EQ(0U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, OnlyOldEntriesEvicted) {
  WifiData older_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr older_geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(older_wifi_data, *older_geoposition);
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());

  // Some time passes, but less than kMaximumLifetime
  task_environment_.FastForwardBy(PositionCacheImpl::kMaximumLifetime * 0.5);

  // Old position is still there.
  const mojom::Geoposition* found_position =
      cache_.FindPosition(older_wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(older_geoposition->Equals(*found_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());

  // New position is added.
  WifiData newer_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr newer_geoposition = testing::CreateGeoposition(2);
  cache_.CachePosition(newer_wifi_data, *newer_geoposition);
  EXPECT_EQ(2U, cache_.GetPositionCacheSize());

  // Enough time passes to evict the older entry, but not enough to evict the
  // newer one.
  task_environment_.FastForwardBy(PositionCacheImpl::kMaximumLifetime * 0.75);

  EXPECT_EQ(nullptr, cache_.FindPosition(older_wifi_data));

  const mojom::Geoposition* found_newer_position =
      cache_.FindPosition(newer_wifi_data);
  ASSERT_NE(nullptr, found_newer_position);
  EXPECT_TRUE(newer_geoposition->Equals(*found_newer_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, NetworkChangeClearsEmptyWifiDataPosition) {
  // Cache a position for non-empty WifiData.
  WifiData initial_wifi_data = testing::CreateDefaultUniqueWifiData();
  mojom::GeopositionPtr initial_geoposition = testing::CreateGeoposition(1);
  cache_.CachePosition(initial_wifi_data, *initial_geoposition);
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());

  // Cache a position for empty WifiData (wired network).
  WifiData empty_wifi_data;
  mojom::GeopositionPtr empty_data_geoposition = testing::CreateGeoposition(2);
  cache_.CachePosition(empty_wifi_data, *empty_data_geoposition);
  EXPECT_EQ(2U, cache_.GetPositionCacheSize());

  cache_.SetLastUsedNetworkPosition(
      *device::mojom::GeopositionResult::NewPosition(
          initial_geoposition.Clone()));

  // When network changes...
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  task_environment_.RunUntilIdle();

  // ... last network position is cleared
  EXPECT_FALSE(cache_.GetLastUsedNetworkPosition());

  // And the position for empty WifiData is cleared, since we're probably on
  // a different wired network now.
  EXPECT_EQ(nullptr, cache_.FindPosition(empty_wifi_data));

  // But the position for non-empty WifiData remains, since our access point
  // scans remain valid even if we moved to a different access point.
  const mojom::Geoposition* found_position =
      cache_.FindPosition(initial_wifi_data);
  ASSERT_NE(nullptr, found_position);
  EXPECT_TRUE(initial_geoposition->Equals(*found_position));
  EXPECT_EQ(1U, cache_.GetPositionCacheSize());
}

TEST_F(PositionCacheImplTest, DiagnosticsEmpty) {
  auto diagnostics = GetDiagnostics();
  ASSERT_TRUE(diagnostics);
  EXPECT_EQ(0u, diagnostics->cache_size);
  EXPECT_FALSE(diagnostics->last_hit);
  EXPECT_FALSE(diagnostics->last_miss);
  EXPECT_FALSE(diagnostics->hit_rate);
  EXPECT_FALSE(diagnostics->last_network_result);
}

TEST_F(PositionCacheImplTest, DiagnosticsCacheMiss) {
  base::Time miss_time = base::Time::Now();
  EXPECT_EQ(nullptr,
            cache_.FindPosition(testing::CreateDefaultUniqueWifiData()));
  auto diagnostics = GetDiagnostics();
  ASSERT_TRUE(diagnostics);
  EXPECT_EQ(0u, diagnostics->cache_size);
  EXPECT_FALSE(diagnostics->last_hit);
  EXPECT_EQ(miss_time, diagnostics->last_miss);
  EXPECT_EQ(0.0, diagnostics->hit_rate);
  EXPECT_FALSE(diagnostics->last_network_result);
}

TEST_F(PositionCacheImplTest, DiagnosticsCacheHit) {
  auto wifi_data = testing::CreateDefaultUniqueWifiData();
  auto position = testing::CreateGeoposition(1);
  cache_.CachePosition(wifi_data, *position);
  {
    auto diagnostics = GetDiagnostics();
    ASSERT_TRUE(diagnostics);
    EXPECT_EQ(1u, diagnostics->cache_size);
    EXPECT_FALSE(diagnostics->last_hit);
    EXPECT_FALSE(diagnostics->last_miss);
    EXPECT_FALSE(diagnostics->hit_rate);
    EXPECT_FALSE(diagnostics->last_network_result);
  }
  base::Time hit_time = base::Time::Now();
  auto* found_position = cache_.FindPosition(wifi_data);
  ASSERT_TRUE(found_position);
  EXPECT_EQ(*position, *found_position);
  {
    auto diagnostics = GetDiagnostics();
    ASSERT_TRUE(diagnostics);
    EXPECT_EQ(1u, diagnostics->cache_size);
    EXPECT_EQ(hit_time, diagnostics->last_hit);
    EXPECT_FALSE(diagnostics->last_miss);
    EXPECT_EQ(1.0, diagnostics->hit_rate);
    EXPECT_FALSE(diagnostics->last_network_result);
  }
}

TEST_F(PositionCacheImplTest, DiagnosticsLastNetworkResult) {
  auto result =
      mojom::GeopositionResult::NewPosition(testing::CreateGeoposition(1));
  cache_.SetLastUsedNetworkPosition(*result);

  auto diagnostics = GetDiagnostics();
  ASSERT_TRUE(diagnostics);
  EXPECT_EQ(0u, diagnostics->cache_size);
  EXPECT_FALSE(diagnostics->last_hit);
  EXPECT_FALSE(diagnostics->last_miss);
  EXPECT_FALSE(diagnostics->hit_rate);
  EXPECT_EQ(*result, *diagnostics->last_network_result);
}

}  // namespace device
