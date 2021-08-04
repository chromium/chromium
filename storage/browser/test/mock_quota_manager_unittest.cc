// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr StorageType kTemporary = StorageType::kTemporary;
constexpr StorageType kPersistent = StorageType::kPersistent;

constexpr QuotaClientType kClientFile = QuotaClientType::kFileSystem;
constexpr QuotaClientType kClientDB = QuotaClientType::kIndexedDatabase;

}  // namespace

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest() : deletion_callback_count_(0) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = new MockSpecialStoragePolicy;
    manager_ = new MockQuotaManager(
        false /* is_incognito */, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(), policy_.get());
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
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
                          const std::set<BucketInfo>& buckets,
                          StorageType type) {
    buckets_ = buckets;
    type_ = type;
    std::move(quit_closure).Run();
  }

  void DeleteBucketData(const BucketInfo& bucket,
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

  int deletion_callback_count() const {
    return deletion_callback_count_;
  }

  MockQuotaManager* manager() const {
    return manager_.get();
  }

  const std::set<BucketInfo>& buckets() const { return buckets_; }

  const StorageType& type() const {
    return type_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<BucketInfo> buckets_;
  StorageType type_;

  base::WeakPtrFactory<MockQuotaManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerTest);
};

TEST_F(MockQuotaManagerTest, BasicBucketManipulation) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  const BucketInfo temp_bucket1 =
      manager()->CreateBucket(kStorageKey1, "temp_host1", kTemporary);
  const BucketInfo perm_bucket1 =
      manager()->CreateBucket(kStorageKey1, "perm_host1", kPersistent);
  const BucketInfo temp_bucket2 =
      manager()->CreateBucket(kStorageKey2, "temp_host2", kTemporary);
  const BucketInfo perm_bucket2 =
      manager()->CreateBucket(kStorageKey2, "perm_host2", kPersistent);

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  manager()->AddBucket(temp_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));

  manager()->AddBucket(perm_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));

  manager()->AddBucket(temp_bucket2, {kClientFile, kClientDB},
                       base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));

  manager()->AddBucket(perm_bucket2, {kClientDB}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, BucketDeletion) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host1:1/"),
      kDefaultBucketName, kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host2:1/"),
      kDefaultBucketName, kPersistent);
  const BucketInfo bucket3 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host3:1/"),
      kDefaultBucketName, kTemporary);

  manager()->AddBucket(bucket1, {kClientFile}, base::Time::Now());
  manager()->AddBucket(bucket2, {kClientFile, kClientDB}, base::Time::Now());
  manager()->AddBucket(bucket3, {kClientFile, kClientDB}, base::Time::Now());

  DeleteBucketData(bucket2, {kClientFile});

  EXPECT_EQ(1, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientDB));

  DeleteBucketData(bucket3, {kClientFile, kClientDB});

  EXPECT_EQ(2, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, ModifiedBuckets) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host1:1/"),
      kDefaultBucketName, kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host2:1/"),
      kDefaultBucketName, kTemporary);

  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::TimeDelta::FromMilliseconds(3600000);
  base::TimeDelta a_minute = base::TimeDelta::FromMilliseconds(60000);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());
  EXPECT_TRUE(buckets().empty());

  manager()->AddBucket(bucket1, {kClientFile}, now - an_hour);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(0UL, buckets().count(bucket2));

  manager()->AddBucket(bucket2, {kClientFile}, now);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(2UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(1UL, buckets().count(bucket2));

  GetModifiedBuckets(kTemporary, then, now);

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(0UL, buckets().count(bucket2));

  GetModifiedBuckets(kTemporary, now - a_minute, now + a_minute);

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(0UL, buckets().count(bucket1));
  EXPECT_EQ(1UL, buckets().count(bucket2));
}
}  // namespace storage
