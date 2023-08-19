// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr StorageType kTemporary = StorageType::kTemporary;
constexpr StorageType kSyncable = StorageType::kSyncable;

constexpr QuotaClientType kClientFile = QuotaClientType::kFileSystem;
constexpr QuotaClientType kClientDB = QuotaClientType::kIndexedDatabase;

bool ContainsBucket(const std::set<BucketLocator>& buckets,
                    const BucketInfo& target_bucket) {
  BucketLocator target_bucket_locator(
      target_bucket.id, target_bucket.storage_key, target_bucket.type,
      target_bucket.name == kDefaultBucketName);
  auto it = buckets.find(target_bucket_locator);
  return it != buckets.end();
}

}  // namespace

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest() : deletion_callback_count_(0) {}

  MockQuotaManagerTest(const MockQuotaManagerTest&) = delete;
  MockQuotaManagerTest& operator=(const MockQuotaManagerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();
    manager_ = base::MakeRefCounted<MockQuotaManager>(
        false /* is_incognito */, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), policy_.get());
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  QuotaErrorOr<BucketInfo> GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name) {
    QuotaErrorOr<BucketInfo> result;
    base::RunLoop run_loop;
    BucketInitParams params(storage_key, bucket_name);
    manager_->UpdateOrCreateBucket(
        params,
        base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket) {
          result = std::move(bucket);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  QuotaErrorOr<BucketInfo> GetOrCreateBucketDeprecated(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      const std::string& bucket_name) {
    QuotaErrorOr<BucketInfo> result;
    base::RunLoop run_loop;
    BucketInitParams params(storage_key, bucket_name);
    manager_->GetOrCreateBucketDeprecated(
        params, type,
        base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket) {
          result = std::move(bucket);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  QuotaErrorOr<BucketInfo> GetBucket(const blink::StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType type) {
    QuotaErrorOr<BucketInfo> result;
    base::RunLoop run_loop;
    manager_->GetBucketByNameUnsafe(
        storage_key, bucket_name, type,
        base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket) {
          result = std::move(bucket);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  QuotaErrorOr<BucketInfo> CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> bucket_future;
    manager_->CreateBucketForTesting(storage_key, bucket_name, type,
                                     bucket_future.GetCallback());
    return bucket_future.Take();
  }

  void GetModifiedBuckets(StorageType type, base::Time begin, base::Time end) {
    base::RunLoop run_loop;
    manager_->GetBucketsModifiedBetween(
        type, begin, end,
        base::BindOnce(&MockQuotaManagerTest::GotModifiedBuckets,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void GotModifiedBuckets(base::OnceClosure quit_closure,
                          const std::set<BucketLocator>& buckets) {
    buckets_ = buckets;
    std::move(quit_closure).Run();
  }

  void DeleteBucketData(const BucketLocator& bucket,
                        QuotaClientTypes quota_client_types) {
    base::RunLoop run_loop;
    manager_->DeleteBucketData(
        bucket, std::move(quota_client_types),
        base::BindOnce(&MockQuotaManagerTest::DeletedBucketData,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DeletedBucketData(base::OnceClosure quit_closure,
                         blink::mojom::QuotaStatusCode status) {
    ++deletion_callback_count_;
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
    std::move(quit_closure).Run();
  }

  void CheckUsageAndQuota(const blink::StorageKey& storage_key,
                          StorageType type,
                          const int64_t expected_usage,
                          const int64_t expected_quota) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode, int64_t, int64_t>
        future;
    manager()->GetUsageAndQuota(storage_key, type, future.GetCallback());

    blink::mojom::QuotaStatusCode status = future.Get<0>();
    int64_t usage = future.Get<1>();
    int64_t quota = future.Get<2>();

    EXPECT_EQ(status, blink::mojom::QuotaStatusCode::kOk);
    EXPECT_EQ(usage, expected_usage);
    EXPECT_EQ(quota, expected_quota);
  }

  int deletion_callback_count() const { return deletion_callback_count_; }

  MockQuotaManager* manager() const { return manager_.get(); }

  const std::set<BucketLocator>& buckets() const { return buckets_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<BucketLocator> buckets_;

  base::WeakPtrFactory<MockQuotaManagerTest> weak_factory_{this};
};

TEST_F(MockQuotaManagerTest, GetOrCreateBucket) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const char kBucketName[] = "bucket_name";

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  ASSERT_OK_AND_ASSIGN(BucketInfo bucket1,
                       GetOrCreateBucket(kStorageKey1, kBucketName));
  EXPECT_EQ(bucket1.storage_key, kStorageKey1);
  EXPECT_EQ(bucket1.name, kBucketName);
  EXPECT_EQ(bucket1.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));

  ASSERT_OK_AND_ASSIGN(BucketInfo bucket2,
                       GetOrCreateBucket(kStorageKey2, kBucketName));
  EXPECT_EQ(bucket2.storage_key, kStorageKey2);
  EXPECT_EQ(bucket2.name, kBucketName);
  EXPECT_EQ(bucket2.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientFile));

  ASSERT_OK_AND_ASSIGN(BucketInfo dupe_bucket,
                       GetOrCreateBucket(kStorageKey1, kBucketName));
  EXPECT_EQ(dupe_bucket, bucket1);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);

  // GetOrCreateBucket actually creates buckets associated with all quota client
  // types, so check them all.
  for (auto client_type : AllQuotaClientTypes()) {
    EXPECT_EQ(manager()->BucketDataCount(client_type), 2);
    EXPECT_TRUE(manager()->BucketHasData(bucket1, client_type));
    EXPECT_TRUE(manager()->BucketHasData(bucket2, client_type));
  }
}

TEST_F(MockQuotaManagerTest, GetOrCreateBucketSync) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const char kBucketName[] = "bucket_name";

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  BucketInitParams params(kStorageKey1, kBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket1,
                       manager()->GetOrCreateBucketSync(params));
  EXPECT_EQ(bucket1.storage_key, kStorageKey1);
  EXPECT_EQ(bucket1.name, kBucketName);
  EXPECT_EQ(bucket1.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));

  params = BucketInitParams(kStorageKey2, kBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket2,
                       manager()->GetOrCreateBucketSync(params));
  EXPECT_EQ(bucket2.storage_key, kStorageKey2);
  EXPECT_EQ(bucket2.name, kBucketName);
  EXPECT_EQ(bucket2.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientFile));

  params = BucketInitParams(kStorageKey1, kBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo dupe_bucket,
                       manager()->GetOrCreateBucketSync(params));
  EXPECT_EQ(dupe_bucket, bucket1);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);

  // GetOrCreateBucket actually creates buckets associated with all quota client
  // types, so check them all.
  for (auto client_type : AllQuotaClientTypes()) {
    EXPECT_EQ(manager()->BucketDataCount(client_type), 2);
    EXPECT_TRUE(manager()->BucketHasData(bucket1, client_type));
    EXPECT_TRUE(manager()->BucketHasData(bucket2, client_type));
  }
}

TEST_F(MockQuotaManagerTest, CreateBucketForTesting) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const char kBucketName[] = "bucket_name";

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket1,
      CreateBucketForTesting(kStorageKey1, kBucketName, kTemporary));
  EXPECT_EQ(bucket1.storage_key, kStorageKey1);
  EXPECT_EQ(bucket1.name, kBucketName);
  EXPECT_EQ(bucket1.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket2,
      CreateBucketForTesting(kStorageKey2, kBucketName, kTemporary));
  EXPECT_EQ(bucket2.storage_key, kStorageKey2);
  EXPECT_EQ(bucket2.name, kBucketName);
  EXPECT_EQ(bucket2.type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientFile));

  ASSERT_OK_AND_ASSIGN(BucketInfo dupe_bucket,
                       GetOrCreateBucket(kStorageKey1, kBucketName));
  EXPECT_EQ(dupe_bucket, bucket1);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
}

TEST_F(MockQuotaManagerTest, GetBucket) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  {
    ASSERT_OK_AND_ASSIGN(BucketInfo created,
                         GetOrCreateBucket(kStorageKey1, kDefaultBucketName));
    ASSERT_OK_AND_ASSIGN(
        BucketInfo fetched,
        GetBucket(kStorageKey1, kDefaultBucketName, kTemporary));
    EXPECT_EQ(fetched, created);
    EXPECT_EQ(fetched.storage_key, kStorageKey1);
    EXPECT_EQ(fetched.name, kDefaultBucketName);
    EXPECT_EQ(fetched.type, kTemporary);
  }

  {
    ASSERT_OK_AND_ASSIGN(BucketInfo created,
                         GetOrCreateBucket(kStorageKey2, kDefaultBucketName));
    ASSERT_OK_AND_ASSIGN(
        BucketInfo fetched,
        GetBucket(kStorageKey2, kDefaultBucketName, kTemporary));
    EXPECT_EQ(fetched, created);
    EXPECT_EQ(fetched.storage_key, kStorageKey2);
    EXPECT_EQ(fetched.name, kDefaultBucketName);
    EXPECT_EQ(fetched.type, kTemporary);
  }

  EXPECT_FALSE(
      GetBucket(kStorageKey1, kDefaultBucketName, kSyncable).has_value());
}

TEST_F(MockQuotaManagerTest, BasicBucketManipulation) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  const BucketInfo temp_bucket1 =
      manager()->CreateBucket({kStorageKey1, "temp_host1"}, kTemporary);
  const BucketInfo sync_bucket1 =
      manager()->CreateBucket({kStorageKey1, kDefaultBucketName}, kSyncable);
  const BucketInfo temp_bucket2 =
      manager()->CreateBucket({kStorageKey2, "temp_host2"}, kTemporary);
  const BucketInfo sync_bucket2 =
      manager()->CreateBucket({kStorageKey2, kDefaultBucketName}, kSyncable);

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  manager()->AddBucket(temp_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));

  manager()->AddBucket(sync_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(sync_bucket1, kClientFile));

  manager()->AddBucket(temp_bucket2, {kClientFile, kClientDB},
                       base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(sync_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));

  manager()->AddBucket(sync_bucket2, {kClientDB}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(sync_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(sync_bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, BucketDeletion) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      {StorageKey::CreateFromStringForTesting("http://host1:1/"),
       kDefaultBucketName},
      kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      {StorageKey::CreateFromStringForTesting("http://host2:1/"),
       kDefaultBucketName},
      kSyncable);
  const BucketInfo bucket3 = manager()->CreateBucket(
      {StorageKey::CreateFromStringForTesting("http://host3:1/"),
       kDefaultBucketName},
      kTemporary);

  manager()->AddBucket(bucket1, {kClientFile}, base::Time::Now());
  manager()->AddBucket(bucket2, {kClientFile, kClientDB}, base::Time::Now());
  manager()->AddBucket(bucket3, {kClientFile, kClientDB}, base::Time::Now());

  DeleteBucketData(bucket2.ToBucketLocator(), {kClientFile});

  EXPECT_EQ(1, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientDB));

  DeleteBucketData(bucket3.ToBucketLocator(), {kClientFile, kClientDB});

  EXPECT_EQ(2, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, ModifiedBuckets) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      {StorageKey::CreateFromStringForTesting("http://host1:1/"),
       kDefaultBucketName},
      kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      {StorageKey::CreateFromStringForTesting("http://host2:1/"),
       kDefaultBucketName},
      kTemporary);

  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::Milliseconds(3600000);
  base::TimeDelta a_minute = base::Milliseconds(60000);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());
  EXPECT_TRUE(buckets().empty());

  manager()->AddBucket(bucket1, {kClientFile}, now - an_hour);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(1UL, buckets().size());
  EXPECT_TRUE(ContainsBucket(buckets(), bucket1));
  EXPECT_FALSE(ContainsBucket(buckets(), bucket2));

  manager()->AddBucket(bucket2, {kClientFile}, now);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(2UL, buckets().size());
  EXPECT_TRUE(ContainsBucket(buckets(), bucket1));
  EXPECT_TRUE(ContainsBucket(buckets(), bucket2));

  GetModifiedBuckets(kTemporary, then, now);

  EXPECT_EQ(1UL, buckets().size());
  EXPECT_TRUE(ContainsBucket(buckets(), bucket1));
  EXPECT_FALSE(ContainsBucket(buckets(), bucket2));

  GetModifiedBuckets(kTemporary, now - a_minute, now + a_minute);

  EXPECT_EQ(1UL, buckets().size());
  EXPECT_FALSE(ContainsBucket(buckets(), bucket1));
  EXPECT_TRUE(ContainsBucket(buckets(), bucket2));
}

TEST_F(MockQuotaManagerTest, QuotaAndUsage) {
  const blink::StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");

  const blink::StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  ASSERT_OK_AND_ASSIGN(BucketInfo result,
                       GetOrCreateBucketDeprecated(storage_key1, kTemporary,
                                                   kDefaultBucketName));
  const BucketLocator storage_key1_temp_default_bucket =
      result.ToBucketLocator();

  ASSERT_OK_AND_ASSIGN(result, GetOrCreateBucketDeprecated(
                                   storage_key1, kTemporary, "non-default"));
  const BucketLocator storage_key1_temp_named_bucket = result.ToBucketLocator();

  ASSERT_OK_AND_ASSIGN(
      result,
      GetOrCreateBucketDeprecated(storage_key1, kSyncable, kDefaultBucketName));
  const BucketLocator storage_key1_sync_default_bucket =
      result.ToBucketLocator();

  ASSERT_OK_AND_ASSIGN(result,
                       GetOrCreateBucketDeprecated(storage_key2, kTemporary,
                                                   kDefaultBucketName));
  const BucketLocator storage_key2_temp_default_bucket =
      result.ToBucketLocator();

  SCOPED_TRACE(
      "Checking default usage and quota for storage_key1 (kTemporary)");
  CheckUsageAndQuota(storage_key1, kTemporary, 0,
                     std::numeric_limits<int64_t>::max());

  manager()->SetQuota(storage_key1, kTemporary, 1000);
  // Add usages in different buckets for the same storage key so that we can
  // ensure that these get added together correctly.
  manager()->UpdateUsage(storage_key1_temp_default_bucket, 10);
  manager()->UpdateUsage(storage_key1_temp_named_bucket, 100);

  // Set a quota for the same storage key using a different type to test that
  // these quotas don't affect one another.
  manager()->SetQuota(storage_key1, kSyncable, 2000);
  // Add usages for buckets tied to the same storage key but using a different
  // type to test that these don't affect one another.
  manager()->UpdateUsage(storage_key1_sync_default_bucket, 20);

  // Set a quota and add usage for a different storage key to test that this
  // doesn't affect the quota and usage of the other storage key.
  manager()->SetQuota(storage_key2, kTemporary, 3000);
  manager()->UpdateUsage(storage_key2_temp_default_bucket, 30);

  SCOPED_TRACE("Checking usage and quota for storage_key1 (kTemporary)");
  CheckUsageAndQuota(storage_key1, kTemporary, 110, 1000);

  SCOPED_TRACE("Checking usage and quota for storage_key1 (kSyncable)");
  CheckUsageAndQuota(storage_key1, kSyncable, 20, 2000);

  SCOPED_TRACE("Checking usage and quota for storage_key2 (kTemporary)");
  CheckUsageAndQuota(storage_key2, kTemporary, 30, 3000);
}

}  // namespace storage
