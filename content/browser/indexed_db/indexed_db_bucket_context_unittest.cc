// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

class IndexedDBBucketContextTest : public testing::Test {
 public:
  IndexedDBBucketContextTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  IndexedDBBucketContextTest(const IndexedDBBucketContextTest&) = delete;
  IndexedDBBucketContextTest& operator=(const IndexedDBBucketContextTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        quota_policy_.get());

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    storage::BucketInfo bucket_info = quota_manager_->CreateBucket(
        storage::BucketInitParams::ForDefaultBucket(
            blink::StorageKey::CreateFromStringForTesting(
                "https://example.com")),
        blink::mojom::StorageType::kTemporary);
    bucket_context_ = std::make_unique<IndexedDBBucketContext>(
        bucket_info, std::make_unique<PartitionedLockManager>(),
        IndexedDBBucketContext::Delegate(),
        std::make_unique<IndexedDBFakeBackingStore>(), quota_manager_proxy_,
        /*io_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());
  }

  void SetQuotaLeft(int64_t quota_manager_response) {
    quota_manager_->SetQuota(bucket_context_->bucket_locator().storage_key,
                             blink::mojom::StorageType::kTemporary,
                             quota_manager_response);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<IndexedDBBucketContext> bucket_context_;
};

TEST_F(IndexedDBBucketContextTest, CanUseDiskSpaceQueuing) {
  // Request space 3 times consecutively. The requests should coalesce.
  SetQuotaLeft(100);

  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<bool> success_future2;
  base::test::TestFuture<bool> success_future3;
  bucket_context_->CheckCanUseDiskSpace(30, success_future.GetCallback());
  bucket_context_->CheckCanUseDiskSpace(30, success_future2.GetCallback());
  bucket_context_->CheckCanUseDiskSpace(50, success_future3.GetCallback());
  ASSERT_FALSE(success_future.IsReady());
  ASSERT_FALSE(success_future2.IsReady());
  ASSERT_FALSE(success_future3.IsReady());

  EXPECT_TRUE(success_future.Get());

  // We know these requests coalesced because only the first request waited
  // (via `Get()`), yet all 3 requests are now ready. The first two requests
  // succeed but the third fails.
  ASSERT_TRUE(success_future2.IsReady());
  ASSERT_TRUE(success_future3.IsReady());
  EXPECT_TRUE(success_future2.Get());
  EXPECT_FALSE(success_future3.Get());
}

TEST_F(IndexedDBBucketContextTest, CanUseDiskSpaceCaching) {
  // Verify the limited authority that IndexedDBBucketContext has to approve
  // disk usage without checking the quota manager. First set the quota manager
  // to report a large amount of disk space, but request even more --- the usage
  // shouldn't be approved.
  constexpr const int64_t kLargeSpace = 120;
  SetQuotaLeft(kLargeSpace);
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace + 1,
                                          success_future.GetCallback());
    EXPECT_FALSE(success_future.IsReady());
    EXPECT_FALSE(success_future.Get());
  }

  // Second, simulate something using up a lot of the quota.
  // `CheckCanUseDiskSpace` will fudge and not check with the QuotaManager, so
  // this usage is also approved.
  SetQuotaLeft(10);
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace / 2 + 1,
                                          success_future.GetCallback());
    EXPECT_TRUE(success_future.IsReady());
    EXPECT_TRUE(success_future.Get());
  }
  // Next, request the same amount of space again. `CheckCanUseDiskSpace` does
  // need to double check with the QuotaManager this time as its limited
  // authority has been exhausted, and hence this usage will not be approved.
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace / 2 + 1,
                                          success_future.GetCallback());
    EXPECT_FALSE(success_future.IsReady());
    EXPECT_FALSE(success_future.Get());
  }

  // Set a large amount of disk space again, request a little.
  SetQuotaLeft(kLargeSpace);
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace / 4,
                                          success_future.GetCallback());
    EXPECT_FALSE(success_future.IsReady());
    EXPECT_TRUE(success_future.Get());
  }
  // Wait for the cached value to expire. The next request should be approved,
  // but only after going to the QuotaManager again.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kBucketSpaceCacheTimeLimit * 2);
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace / 4,
                                          success_future.GetCallback());
    EXPECT_FALSE(success_future.IsReady());
    EXPECT_TRUE(success_future.Get());
  }
}

TEST_F(IndexedDBBucketContextTest, CanUseDiskSpaceWarmUp) {
  constexpr const int64_t kLargeSpace = 120;
  SetQuotaLeft(kLargeSpace);

  bucket_context_->CheckCanUseDiskSpace(120, {});
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<bool> success_future;
  bucket_context_->CheckCanUseDiskSpace(120, success_future.GetCallback());
  EXPECT_TRUE(success_future.IsReady());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(IndexedDBBucketContextTest, BucketSpaceDecay) {
  constexpr const int64_t kLargeSpace = 120;
  SetQuotaLeft(kLargeSpace);

  base::test::TestFuture<bool> success_future;
  bucket_context_->CheckCanUseDiskSpace(1, success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  const int64_t can_allot = bucket_context_->GetBucketSpaceToAllot();
  EXPECT_LE(can_allot, 120);

  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kBucketSpaceCacheTimeLimit / 2);
  const int64_t decayed_can_allot = bucket_context_->GetBucketSpaceToAllot();
  EXPECT_LT(decayed_can_allot, can_allot);
  EXPECT_GT(decayed_can_allot, 0);

  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kBucketSpaceCacheTimeLimit / 2);
  EXPECT_EQ(bucket_context_->GetBucketSpaceToAllot(), 0);
}

}  // namespace content
