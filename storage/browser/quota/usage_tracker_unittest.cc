// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::blink::StorageKey;
using ::blink::mojom::QuotaStatusCode;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

class UsageTrackerTestQuotaClient : public mojom::QuotaClient {
 public:
  UsageTrackerTestQuotaClient() = default;

  UsageTrackerTestQuotaClient(const UsageTrackerTestQuotaClient&) = delete;
  UsageTrackerTestQuotaClient& operator=(const UsageTrackerTestQuotaClient&) =
      delete;

  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, bucket.type);
    int64_t usage = GetUsage(bucket);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), usage));
  }

  void GetStorageKeysForType(StorageType type,
                             GetStorageKeysForTypeCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::set<StorageKey> storage_keys;
    for (const auto& bucket_usage_pair : bucket_usage_map_) {
      storage_keys.emplace(bucket_usage_pair.first.storage_key);
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<StorageKey>(storage_keys.begin(),
                                                          storage_keys.end())));
  }

  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, bucket.type);
    bucket_usage_map_.erase(bucket);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), QuotaStatusCode::kOk));
  }

  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override {
    std::move(callback).Run();
  }

  int64_t GetUsage(const BucketLocator& bucket) {
    auto it = bucket_usage_map_.find(bucket);
    if (it == bucket_usage_map_.end()) {
      return 0;
    }
    return it->second;
  }

  int64_t UpdateUsage(const BucketLocator& bucket, int64_t delta) {
    return bucket_usage_map_[bucket] += delta;
  }

 private:
  std::map<BucketLocator, int64_t> bucket_usage_map_;
};

std::pair<int64_t, blink::mojom::UsageBreakdownPtr> to_pair(
    std::tuple<int64_t, blink::mojom::UsageBreakdownPtr> t) {
  return std::make_pair(std::get<0>(t), std::move(std::get<1>(t)));
}

}  // namespace

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_client_(std::make_unique<UsageTrackerTestQuotaClient>()) {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<QuotaManagerImpl>(
        /*is_incognito=*/false, base_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*quota_change_callback=*/base::DoNothing(), storage_policy_.get(),
        GetQuotaSettingsFunc());
    usage_tracker_ = std::make_unique<UsageTracker>(
        quota_manager_.get(), GetQuotaClientMap(), StorageType::kTemporary,
        storage_policy_.get());
  }

  UsageTrackerTest(const UsageTrackerTest&) = delete;
  UsageTrackerTest& operator=(const UsageTrackerTest&) = delete;

  ~UsageTrackerTest() override = default;

  void UpdateUsage(const BucketLocator& bucket, int64_t delta) {
    quota_client_->UpdateUsage(bucket, delta);
    usage_tracker_->UpdateBucketUsageCache(QuotaClientType::kFileSystem, bucket,
                                           delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const BucketLocator& bucket,
                                      int64_t delta) {
    quota_client_->UpdateUsage(bucket, delta);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    base::test::TestFuture<int64_t, int64_t> future;
    usage_tracker_->GetGlobalUsage(future.GetCallback());
    *usage = future.Get<0>();
    *unlimited_usage = future.Get<1>();
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr>
  GetStorageKeyUsageWithBreakdown(const blink::StorageKey& storage_key) {
    base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr> future;
    usage_tracker_->GetStorageKeyUsageWithBreakdown(storage_key,
                                                    future.GetCallback());
    return to_pair(future.Take());
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr>
  GetBucketUsageWithBreakdown(const BucketLocator& bucket) {
    base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr> future;
    usage_tracker_->GetBucketUsageWithBreakdown(bucket, future.GetCallback());
    return to_pair(future.Take());
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
    usage_tracker_->SetUsageCacheEnabled(QuotaClientType::kFileSystem,
                                         storage_key, enabled);
  }

  BucketLocator CreateBucket(const StorageKey& storage_key,
                             const std::string& bucket_name) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_->CreateBucketForTesting(storage_key, bucket_name,
                                           StorageType::kTemporary,
                                           future.GetCallback());
    QuotaErrorOr<BucketInfo> bucket_result = future.Take();
    DCHECK(bucket_result.has_value());
    return bucket_result.value().ToBucketLocator();
  }

  void OpenDatabase() { quota_manager_->EnsureDatabaseOpened(); }

  void DisableQuotaDatabase() {
    base::RunLoop run_loop;
    quota_manager_->PostTaskAndReplyWithResultForDBThread(
        base::BindLambdaForTesting([&](QuotaDatabase* db) {
          db->SetDisabledForTesting(true);
          return QuotaError::kNone;
        }),
        base::BindLambdaForTesting([&](QuotaError error) { run_loop.Quit(); }),
        FROM_HERE, /*is_bootstrap_task=*/false);
    run_loop.Run();
  }

  void disable_database_bootstrap(bool disable) {
    quota_manager_->SetBootstrapDisabledForTesting(disable);
  }

  UsageTracker* usage_tracker() { return usage_tracker_.get(); }

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

  scoped_refptr<QuotaManagerImpl> quota_manager_;
  std::unique_ptr<UsageTracker> usage_tracker_;
  base::ScopedTempDir base_;
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr storage_key_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  blink::mojom::UsageBreakdownPtr bucket_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");

  BucketLocator bucket = CreateBucket(storage_key, kDefaultBucketName);

  UpdateUsage(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  storage_key_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr>
      storage_key_usage_breakdown =
          GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(100, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> bucket_usage_breakdown =
      GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(100, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  storage_key_usage_breakdown = GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(100, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown = GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(100, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(100, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(100, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr storage_key_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  blink::mojom::UsageBreakdownPtr bucket_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example.com");

  BucketLocator bucket = CreateBucket(storage_key, kDefaultBucketName);

  UpdateUsage(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  storage_key_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr>
      storage_key_usage_breakdown =
          GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(100, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> bucket_usage_breakdown =
      GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(100, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  UpdateUsageWithoutNotification(bucket, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  storage_key_usage_breakdown = GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(100, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown = GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(100, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(storage_key);
  UpdateUsageWithoutNotification(bucket, 100);
  SetUsageCacheEnabled(storage_key, false);
  UpdateUsageWithoutNotification(bucket, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  storage_key_usage_breakdown = GetStorageKeyUsageWithBreakdown(storage_key);
  storage_key_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(400, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown = GetBucketUsageWithBreakdown(bucket);
  bucket_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(400, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(storage_key);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  storage_key_usage_breakdown = GetStorageKeyUsageWithBreakdown(storage_key);
  EXPECT_EQ(400, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown = GetBucketUsageWithBreakdown(bucket);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);

  SetUsageCacheEnabled(storage_key, true);
  UpdateUsage(bucket, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  storage_key_usage_breakdown = GetStorageKeyUsageWithBreakdown(storage_key);
  storage_key_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(500, storage_key_usage_breakdown.first);
  EXPECT_EQ(storage_key_usage_breakdown_expected,
            storage_key_usage_breakdown.second);
  bucket_usage_breakdown = GetBucketUsageWithBreakdown(bucket);
  bucket_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(500, bucket_usage_breakdown.first);
  EXPECT_EQ(bucket_usage_breakdown_expected, bucket_usage_breakdown.second);
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

  BucketLocator bucket_normal = CreateBucket(kNormal, kDefaultBucketName);
  BucketLocator bucket_unlimited = CreateBucket(kUnlimited, kDefaultBucketName);
  BucketLocator bucket_noncached = CreateBucket(kNonCached, kDefaultBucketName);
  BucketLocator bucket_noncached_unlimited =
      CreateBucket(kNonCachedUnlimited, kDefaultBucketName);

  GrantUnlimitedStoragePolicy(kUnlimited);
  GrantUnlimitedStoragePolicy(kNonCachedUnlimited);

  SetUsageCacheEnabled(kNonCached, false);
  SetUsageCacheEnabled(kNonCachedUnlimited, false);

  UpdateUsageWithoutNotification(bucket_normal, 1);
  UpdateUsageWithoutNotification(bucket_unlimited, 2);
  UpdateUsageWithoutNotification(bucket_noncached, 4);
  UpdateUsageWithoutNotification(bucket_noncached_unlimited, 8);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 4 + 8, total_usage);
  EXPECT_EQ(2 + 8, unlimited_usage);

  UpdateUsageWithoutNotification(bucket_noncached, 16 - 4);
  UpdateUsageWithoutNotification(bucket_noncached_unlimited, 32 - 8);

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

  BucketLocator bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketLocator bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

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

  BucketLocator bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketLocator bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the storage keys after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  UpdateUsage(bucket1, 100);
  UpdateUsage(bucket2, 200);

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

  BucketLocator bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketLocator bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

  SetUsageCacheEnabled(kStorageKey1, false);
  SetUsageCacheEnabled(kStorageKey2, false);

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

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

  BucketLocator bucket1 = CreateBucket(kStorageKey1, kDefaultBucketName);
  BucketLocator bucket2 = CreateBucket(kStorageKey2, kDefaultBucketName);

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

  UpdateUsageWithoutNotification(bucket1, 100);
  UpdateUsageWithoutNotification(bucket2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, QuotaDatabaseDisabled) {
  disable_database_bootstrap(true);
  OpenDatabase();

  DisableQuotaDatabase();

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(total_usage, -1);
  EXPECT_EQ(unlimited_usage, -1);

  const StorageKey kStorageKey =
      StorageKey::CreateFromStringForTesting("http://example.com");
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr>
      storage_key_usage_breakdown =
          GetStorageKeyUsageWithBreakdown(kStorageKey);
  EXPECT_EQ(storage_key_usage_breakdown.first, -1);
  EXPECT_EQ(storage_key_usage_breakdown.second,
            blink::mojom::UsageBreakdown::New());
}

TEST_F(UsageTrackerTest, IsWorking) {
  const StorageKey kStorageKey =
      StorageKey::CreateFromStringForTesting("http://example.com");
  BucketLocator bucket = CreateBucket(kStorageKey, kDefaultBucketName);
  UpdateUsageWithoutNotification(bucket, 100);

  EXPECT_FALSE(usage_tracker()->IsWorking());

  // UsageTracker::GetBucketUsage task.
  base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr>
      bucket_usage_future;
  usage_tracker()->GetBucketUsageWithBreakdown(
      bucket, bucket_usage_future.GetCallback());

  EXPECT_TRUE(usage_tracker()->IsWorking());

  ASSERT_TRUE(bucket_usage_future.Wait());
  EXPECT_FALSE(usage_tracker()->IsWorking());

  // UsageTracker::GetStorageKeyUsage task.
  base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr>
      storage_key_usage_future;
  usage_tracker()->GetStorageKeyUsageWithBreakdown(
      kStorageKey, storage_key_usage_future.GetCallback());

  EXPECT_TRUE(usage_tracker()->IsWorking());

  ASSERT_TRUE(storage_key_usage_future.Wait());
  EXPECT_FALSE(usage_tracker()->IsWorking());

  // UsageTracker::GetGlobalUsage task.
  base::test::TestFuture<int64_t, int64_t> global_usage_future;
  usage_tracker()->GetGlobalUsage(global_usage_future.GetCallback());

  EXPECT_TRUE(usage_tracker()->IsWorking());

  ASSERT_TRUE(global_usage_future.Wait());
  EXPECT_FALSE(usage_tracker()->IsWorking());
}

}  // namespace storage
