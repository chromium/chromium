// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::blink::StorageKey;
using ::blink::mojom::QuotaStatusCode;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

void DidGetGlobalUsage(bool* done,
                       int64_t* usage_out,
                       int64_t* unlimited_usage_out,
                       int64_t usage,
                       int64_t unlimited_usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
  *unlimited_usage_out = unlimited_usage;
}

class UsageTrackerTestQuotaClient : public mojom::QuotaClient {
 public:
  UsageTrackerTestQuotaClient() = default;

  void GetStorageKeyUsage(const StorageKey& storage_key,
                          StorageType type,
                          GetStorageKeyUsageCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    int64_t usage = GetUsage(storage_key);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), usage));
  }

  void GetStorageKeysForType(StorageType type,
                             GetStorageKeysForTypeCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::vector<StorageKey> storage_keys;
    for (const auto& storage_key_usage_pair : storage_key_usage_map_)
      storage_keys.push_back(storage_key_usage_pair.first);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(storage_keys)));
  }

  void GetStorageKeysForHost(StorageType type,
                             const std::string& host,
                             GetStorageKeysForHostCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::vector<StorageKey> storage_keys;
    for (const auto& storage_key_usage_pair : storage_key_usage_map_) {
      if (storage_key_usage_pair.first.origin().host() == host)
        storage_keys.push_back(storage_key_usage_pair.first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(storage_keys)));
  }

  void DeleteStorageKeyData(const StorageKey& storage_key,
                            StorageType type,
                            DeleteStorageKeyDataCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    storage_key_usage_map_.erase(storage_key);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), QuotaStatusCode::kOk));
  }

  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override {
    std::move(callback).Run();
  }

  int64_t GetUsage(const StorageKey& storage_key) {
    auto it = storage_key_usage_map_.find(storage_key);
    if (it == storage_key_usage_map_.end())
      return 0;
    return it->second;
  }

  void SetUsage(const StorageKey& storage_key, int64_t usage) {
    storage_key_usage_map_[storage_key] = usage;
  }

  int64_t UpdateUsage(const StorageKey& storage_key, int64_t delta) {
    return storage_key_usage_map_[storage_key] += delta;
  }

 private:
  std::map<StorageKey, int64_t> storage_key_usage_map_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTestQuotaClient);
};

}  // namespace

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_client_(std::make_unique<UsageTrackerTestQuotaClient>()),
        usage_tracker_(GetQuotaClientMap(),
                       StorageType::kTemporary,
                       storage_policy_.get()) {}

  ~UsageTrackerTest() override = default;

  UsageTracker* usage_tracker() {
    return &usage_tracker_;
  }

  static void DidGetUsageBreakdown(
      bool* done,
      int64_t* usage_out,
      blink::mojom::UsageBreakdownPtr* usage_breakdown_out,
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    EXPECT_FALSE(*done);
    *usage_out = usage;
    *usage_breakdown_out = std::move(usage_breakdown);
    *done = true;
  }

  void UpdateUsage(const StorageKey& storage_key, int64_t delta) {
    quota_client_->UpdateUsage(storage_key, delta);
    usage_tracker_.UpdateUsageCache(QuotaClientType::kFileSystem, storage_key,
                                    delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const StorageKey& storage_key,
                                      int64_t delta) {
    quota_client_->UpdateUsage(storage_key, delta);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalUsage(
        base::BindOnce(&DidGetGlobalUsage, &done, usage, unlimited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> GetHostUsageWithBreakdown(
      const std::string& host) {
    int64_t usage;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
    bool done = false;

    usage_tracker_.GetHostUsageWithBreakdown(
        host, base::BindOnce(&UsageTrackerTest::DidGetUsageBreakdown, &done,
                             &usage, &usage_breakdown));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(done);
    return std::make_pair(usage, std::move(usage_breakdown));
  }

  void GrantUnlimitedStoragePolicy(const StorageKey& storage_key) {
    if (!storage_policy_->IsStorageUnlimited(storage_key.origin().GetURL())) {
      storage_policy_->AddUnlimited(storage_key.origin().GetURL());
      storage_policy_->NotifyGranted(storage_key.origin(),
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void RevokeUnlimitedStoragePolicy(const StorageKey& storage_key) {
    if (storage_policy_->IsStorageUnlimited(storage_key.origin().GetURL())) {
      storage_policy_->RemoveUnlimited(storage_key.origin().GetURL());
      storage_policy_->NotifyRevoked(storage_key.origin(),
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void SetUsageCacheEnabled(const StorageKey& storage_key, bool enabled) {
    usage_tracker_.SetUsageCacheEnabled(QuotaClientType::kFileSystem,
                                        storage_key, enabled);
  }

 private:
  base::flat_map<mojom::QuotaClient*, QuotaClientType> GetQuotaClientMap() {
    base::flat_map<mojom::QuotaClient*, QuotaClientType> client_map;
    client_map.insert(
        std::make_pair(quota_client_.get(), QuotaClientType::kFileSystem));
    return client_map;
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  std::unique_ptr<UsageTrackerTestQuotaClient> quota_client_;
  UsageTracker usage_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTest);
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const std::string& host = storage_key.origin().host();

  UpdateUsage(storage_key, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const std::string& host = storage_key.origin().host();

  UpdateUsage(storage_key, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  UpdateUsageWithoutNotification(storage_key, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  UpdateUsageWithoutNotification(storage_key, 100);
  SetUsageCacheEnabled(storage_key, false);
  UpdateUsageWithoutNotification(storage_key, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  SetUsageCacheEnabled(storage_key, true);
  UpdateUsage(storage_key, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(500, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, GlobalUsageUnlimitedUncached) {
  const StorageKey kNormal =
      StorageKey::CreateFromStringForTesting("http://normal");
  const StorageKey kUnlimited =
      StorageKey::CreateFromStringForTesting("http://unlimited");
  const StorageKey kNonCached =
      StorageKey::CreateFromStringForTesting("http://non_cached");
  const StorageKey kNonCachedUnlimited =
      StorageKey::CreateFromStringForTesting("http://non_cached-unlimited");

  GrantUnlimitedStoragePolicy(kUnlimited);
  GrantUnlimitedStoragePolicy(kNonCachedUnlimited);

  SetUsageCacheEnabled(kNonCached, false);
  SetUsageCacheEnabled(kNonCachedUnlimited, false);

  UpdateUsageWithoutNotification(kNormal, 1);
  UpdateUsageWithoutNotification(kUnlimited, 2);
  UpdateUsageWithoutNotification(kNonCached, 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 8);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 4 + 8, total_usage);
  EXPECT_EQ(2 + 8, unlimited_usage);

  UpdateUsageWithoutNotification(kNonCached, 16 - 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 32 - 8);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 16 + 32, total_usage);
  EXPECT_EQ(2 + 32, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostCachedInit) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  UpdateUsageWithoutNotification(kStorageKey1, 100);
  UpdateUsageWithoutNotification(kStorageKey2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the storage_keys before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostCachedUpdate) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the storage keys after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  UpdateUsage(kStorageKey1, 100);
  UpdateUsage(kStorageKey2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostUncachedInit) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  SetUsageCacheEnabled(kStorageKey1, false);
  SetUsageCacheEnabled(kStorageKey2, false);

  UpdateUsageWithoutNotification(kStorageKey1, 100);
  UpdateUsageWithoutNotification(kStorageKey2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the storage keys before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleStorageKeysPerHostUncachedUpdate) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://example.com");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://example.com:8080");
  ASSERT_EQ(kStorageKey1.origin().host(), kStorageKey2.origin().host())
      << "The test assumes that the two storage keys have the same host";

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the storage keys after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  SetUsageCacheEnabled(kStorageKey1, false);
  SetUsageCacheEnabled(kStorageKey2, false);

  UpdateUsageWithoutNotification(kStorageKey1, 100);
  UpdateUsageWithoutNotification(kStorageKey2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

}  // namespace storage
