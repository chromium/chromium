// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/connect_predictor_lru_cache.h"

#include <optional>
#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {
namespace {

constexpr IPAddress kIPv4_1(192, 168, 1, 1);
constexpr IPAddress kIPv4_1_SameSubnet(192, 168, 1, 100);
constexpr IPAddress kIPv4_2(192, 168, 2, 1);

constexpr IPAddress
    kIPv6_1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
constexpr IPAddress
    kIPv6_1_SameSubnet(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 99, 99, 99, 99);
constexpr IPAddress
    kIPv6_2(1, 2, 3, 4, 5, 6, 7, 99, 9, 10, 11, 13, 13, 14, 15, 16);

constexpr IPAddress
    kIPv4MappedIPv6_1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 192, 168, 1, 1);
constexpr IPAddress kIPv4MappedIPv6_1_SameSubnet(0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 255,
                                                 255,
                                                 192,
                                                 168,
                                                 1,
                                                 100);
constexpr IPAddress
    kIPv4MappedIPv6_2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 192, 168, 2, 1);

constexpr IPAddress kEmptyIPAddress;

class ConnectPredictorLRUCacheTest : public ::testing::Test {
 public:
  ConnectPredictorLRUCacheTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAddressSorterConnectCache,
        {
            {"AddressSorterConnectCacheMaxNetworks", "2"},
            {"AddressSorterConnectCacheMaxNaksPerNetwork", "2"},
            {"AddressSorterConnectCacheMaxPredictionsPerPartition", "2"},
        });
    network_change_notifier_ = NetworkChangeNotifier::CreateMockIfNeeded();
  }

  ~ConnectPredictorLRUCacheTest() override {
    // To avoid flakiness, ensure no threads are being created at destruction
    // time. The presubmit will complain about this, but here it is safe and
    // correct.
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  // `task_environment_` needs to be destroyed before `feature_list_` which is
  // why this fixture cannot use TestWithTaskEnvironment.
  NetTaskEnvironment task_environment_;
  std::unique_ptr<NetworkChangeNotifier> network_change_notifier_;
};

TEST_F(ConnectPredictorLRUCacheTest, MaskIPv4) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kIPv4_1, {OK, kIPv4_1});

  EXPECT_TRUE(partition->Predict(kIPv4_1).has_value());
  EXPECT_TRUE(partition->Predict(kIPv4_1_SameSubnet).has_value());
  EXPECT_FALSE(partition->Predict(kIPv4_2).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, MaskIPv6) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kIPv6_1, {OK, kIPv6_1});

  EXPECT_TRUE(partition->Predict(kIPv6_1).has_value());
  EXPECT_TRUE(partition->Predict(kIPv6_1_SameSubnet).has_value());
  EXPECT_FALSE(partition->Predict(kIPv6_2).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, MaskIPv4MappedIPv6) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kIPv4MappedIPv6_1, {OK, kIPv4MappedIPv6_1});

  EXPECT_TRUE(partition->Predict(kIPv4MappedIPv6_1).has_value());
  EXPECT_TRUE(partition->Predict(kIPv4MappedIPv6_1_SameSubnet).has_value());
  EXPECT_FALSE(partition->Predict(kIPv4MappedIPv6_2).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, EmptyIPAddress) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kEmptyIPAddress, {OK, kEmptyIPAddress});
  EXPECT_TRUE(partition->Predict(kEmptyIPAddress).has_value());
  EXPECT_FALSE(partition->Predict(kIPv4_1).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, PredictionsLRUEviction) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kIPv4_1, {OK, kIPv4_1});
  partition->RecordResult(kIPv4_2, {OK, kIPv4_2});

  // Cache is full (size 2). Evict kIPv4_1 by adding another.
  IPAddress ipv4_3(192, 168, 3, 1);
  partition->RecordResult(ipv4_3, {OK, ipv4_3});

  EXPECT_FALSE(partition->Predict(kIPv4_1).has_value());
  EXPECT_TRUE(partition->Predict(kIPv4_2).has_value());
  EXPECT_TRUE(partition->Predict(ipv4_3).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, NakLRUEviction) {
  ConnectPredictorLRUCache cache;

  auto nak1 = NetworkAnonymizationKey::CreateTransient();
  auto nak2 = NetworkAnonymizationKey::CreateTransient();
  auto nak3 = NetworkAnonymizationKey::CreateTransient();

  auto p1 = cache.GetPartition(handles::kInvalidNetworkHandle, nak1);
  p1->RecordResult(kIPv4_1, {OK, kIPv4_1});

  auto p2 = cache.GetPartition(handles::kInvalidNetworkHandle, nak2);
  p2->RecordResult(kIPv4_1, {OK, kIPv4_1});

  // Cache is full (size 2). Nak1 partition should be evicted.
  auto p3 = cache.GetPartition(handles::kInvalidNetworkHandle, nak3);
  p3->RecordResult(kIPv4_1, {OK, kIPv4_1});

  // nak2 and nak3 should still have their predictions.
  EXPECT_TRUE(cache.GetPartition(handles::kInvalidNetworkHandle, nak2)
                  ->Predict(kIPv4_1)
                  .has_value());
  EXPECT_TRUE(cache.GetPartition(handles::kInvalidNetworkHandle, nak3)
                  ->Predict(kIPv4_1)
                  .has_value());

  // Re-requesting nak1 should yield a fresh partition without kIPv4_1.
  auto new_p1 = cache.GetPartition(handles::kInvalidNetworkHandle, nak1);
  EXPECT_FALSE(new_p1->Predict(kIPv4_1).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, NetworkHandleLRUEviction) {
  ConnectPredictorLRUCache cache;

  NetworkAnonymizationKey nak = NetworkAnonymizationKey::CreateTransient();

  auto p1 = cache.GetPartition(1, nak);
  p1->RecordResult(kIPv4_1, {OK, kIPv4_1});

  auto p2 = cache.GetPartition(2, nak);
  p2->RecordResult(kIPv4_1, {OK, kIPv4_1});

  // Cache is full (size 2). Handle 1 should be evicted.
  auto p3 = cache.GetPartition(3, nak);
  p3->RecordResult(kIPv4_1, {OK, kIPv4_1});

  // Handle 2 and 3 should still have their predictions.
  EXPECT_TRUE(cache.GetPartition(2, nak)->Predict(kIPv4_1).has_value());
  EXPECT_TRUE(cache.GetPartition(3, nak)->Predict(kIPv4_1).has_value());

  // Re-requesting handle 1 should yield a fresh partition without kIPv4_1.
  auto new_p1 = cache.GetPartition(1, nak);
  EXPECT_FALSE(new_p1->Predict(kIPv4_1).has_value());
}

TEST_F(ConnectPredictorLRUCacheTest, ClearOnNetworkChange) {
  ConnectPredictorLRUCache cache;
  auto partition = cache.GetPartition(handles::kInvalidNetworkHandle,
                                      NetworkAnonymizationKey());

  partition->RecordResult(kIPv4_1, {OK, kIPv4_1});
  EXPECT_TRUE(partition->Predict(kIPv4_1).has_value());

  NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      NetworkChangeNotifier::CONNECTION_WIFI);

  // We need to run the message loop for the notification to be delivered.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !cache
                .GetPartition(handles::kInvalidNetworkHandle,
                              NetworkAnonymizationKey())
                ->Predict(kIPv4_1)
                .has_value();
  }));
}

TEST_F(ConnectPredictorLRUCacheTest, CacheHitDoesNotRecreatePartitions) {
  ConnectPredictorLRUCache cache;
  NetworkAnonymizationKey nak = NetworkAnonymizationKey::CreateTransient();

  auto p1 = cache.GetPartition(handles::kInvalidNetworkHandle, nak);
  p1->RecordResult(kIPv4_1, {OK, kIPv4_1});

  auto p2 = cache.GetPartition(handles::kInvalidNetworkHandle, nak);
  EXPECT_EQ(p1.get(), p2.get());
  EXPECT_TRUE(p2->Predict(kIPv4_1).has_value());
}

}  // namespace
}  // namespace net
