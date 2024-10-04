// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/bucket_context.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom-shared.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom.h"
#include "content/browser/indexed_db/instance/fake_transaction.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content::indexed_db {

using testing::SizeIs;
using ITS = storage::mojom::IdbTransactionState;

class BucketContextTest : public testing::Test {
 public:
  BucketContextTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  BucketContextTest(const BucketContextTest&) = delete;
  BucketContextTest& operator=(const BucketContextTest&) = delete;

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
    bucket_context_ = std::make_unique<BucketContext>(
        bucket_info, base::FilePath(), BucketContext::Delegate(),
        quota_manager_proxy_,
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
  std::unique_ptr<BucketContext> bucket_context_;
};

TEST_F(BucketContextTest, CanUseDiskSpaceQueuing) {
  base::HistogramTester tester;
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

  tester.ExpectTotalCount("IndexedDB.QuotaCheckTime2.Success", 1);
}

TEST_F(BucketContextTest, CanUseDiskSpaceCaching) {
  // Verify the limited authority that BucketContext has to approve
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
  task_environment_.FastForwardBy(BucketContext::kBucketSpaceCacheTimeLimit *
                                  2);
  {
    base::test::TestFuture<bool> success_future;
    bucket_context_->CheckCanUseDiskSpace(kLargeSpace / 4,
                                          success_future.GetCallback());
    EXPECT_FALSE(success_future.IsReady());
    EXPECT_TRUE(success_future.Get());
  }
}

TEST_F(BucketContextTest, CanUseDiskSpaceWarmUp) {
  constexpr const int64_t kLargeSpace = 120;
  SetQuotaLeft(kLargeSpace);

  bucket_context_->CheckCanUseDiskSpace(120, {});
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<bool> success_future;
  bucket_context_->CheckCanUseDiskSpace(120, success_future.GetCallback());
  EXPECT_TRUE(success_future.IsReady());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(BucketContextTest, BucketSpaceDecay) {
  constexpr const int64_t kLargeSpace = 120;
  SetQuotaLeft(kLargeSpace);

  base::test::TestFuture<bool> success_future;
  bucket_context_->CheckCanUseDiskSpace(1, success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  const int64_t can_allot = bucket_context_->GetBucketSpaceToAllot();
  EXPECT_LE(can_allot, 120);

  task_environment_.FastForwardBy(BucketContext::kBucketSpaceCacheTimeLimit /
                                  2);
  const int64_t decayed_can_allot = bucket_context_->GetBucketSpaceToAllot();
  EXPECT_LT(decayed_can_allot, can_allot);
  EXPECT_GT(decayed_can_allot, 0);

  task_environment_.FastForwardBy(BucketContext::kBucketSpaceCacheTimeLimit /
                                  2);
  EXPECT_EQ(bucket_context_->GetBucketSpaceToAllot(), 0);
}

// Verifies state history is calculated correctly based on snapshots.
TEST_F(BucketContextTest, MetadataRecordingStateHistory) {
  bucket_context_->StartMetadataRecording();

  // Helper function to make a idb internals metadata snapshot with a single
  // transaction.
  auto make_snapshot = [](std::u16string db_name, int64_t tid, ITS state,
                          double age) {
    storage::mojom::IdbBucketMetadataPtr metadata =
        storage::mojom::IdbBucketMetadata::New();
    auto db = storage::mojom::IdbDatabaseMetadata::New();
    db->name = db_name;
    auto tx = storage::mojom::IdbTransactionMetadata::New();
    tx->tid = tid;
    tx->state = state;
    tx->age = age;
    tx->connection_id = 0;
    db->transactions.push_back(std::move(tx));
    metadata->databases.push_back(std::move(db));

    // Add to a second parallel connection_id to test that it doesn't interfere.
    db = storage::mojom::IdbDatabaseMetadata::New();
    db->name = db_name;
    tx = storage::mojom::IdbTransactionMetadata::New();
    tx->connection_id = 1;
    tx->tid = tid;
    tx->state = state;
    tx->age = age * 2;
    db->transactions.push_back(std::move(tx));
    metadata->databases.push_back(std::move(db));

    return metadata;
  };

  bucket_context_->metadata_recording_buffer_.push_back(
      make_snapshot(u"database0", /* tid = */ 1, ITS::kStarted,
                    /* age = */ 0));

  // Add another transaction with a different tid to ensure it does not
  // interfere.
  auto tx = storage::mojom::IdbTransactionMetadata::New();
  tx->tid = 2;
  tx->state = ITS::kRunning;
  tx->age = 4;
  bucket_context_->metadata_recording_buffer_[1]
      ->databases[0]
      ->transactions.push_back(std::move(tx));

  bucket_context_->metadata_recording_buffer_.push_back(
      make_snapshot(u"database0", /* tid = */ 1, ITS::kRunning,
                    /* age = */ 10));
  bucket_context_->metadata_recording_buffer_.push_back(
      make_snapshot(u"database0", /* tid = */ 1, ITS::kCommitting,
                    /* age = */ 20));
  bucket_context_->metadata_recording_buffer_.push_back(
      make_snapshot(u"database0", /* tid = */ 1, ITS::kRunning,
                    /* age = */ 30));
  bucket_context_->metadata_recording_buffer_.push_back(
      make_snapshot(u"database0", /* tid = */ 1, ITS::kFinished,
                    /* age = */ 50));

  std::vector<storage::mojom::IdbBucketMetadataPtr> result =
      bucket_context_->StopMetadataRecording();
  EXPECT_THAT(result, SizeIs(6));

  // Snapshot 0
  EXPECT_THAT(result[0]->databases, SizeIs(0));

  // Snapshot 1.
  tx = std::move(result[1]->databases[0]->transactions[0]);
  EXPECT_THAT(tx->state_history, SizeIs(1));
  EXPECT_EQ(tx->state_history[0]->state, ITS::kStarted);
  EXPECT_EQ(tx->state_history[0]->duration, 0);

  // Snapshot 2.
  tx = std::move(result[2]->databases[0]->transactions[0]);
  EXPECT_THAT(tx->state_history, SizeIs(2));
  EXPECT_EQ(tx->state_history[0]->state, ITS::kStarted);
  EXPECT_EQ(tx->state_history[0]->duration, 10);
  EXPECT_EQ(tx->state_history[1]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[1]->duration, 0);

  // Snapshot 3.
  tx = std::move(result[3]->databases[0]->transactions[0]);
  EXPECT_THAT(tx->state_history, SizeIs(3));
  EXPECT_EQ(tx->state_history[0]->state, ITS::kStarted);
  EXPECT_EQ(tx->state_history[0]->duration, 10);
  EXPECT_EQ(tx->state_history[1]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[1]->duration, 10);
  EXPECT_EQ(tx->state_history[2]->state, ITS::kCommitting);
  EXPECT_EQ(tx->state_history[2]->duration, 0);

  // Snapshot 4.
  tx = std::move(result[4]->databases[0]->transactions[0]);
  EXPECT_THAT(tx->state_history, SizeIs(4));
  EXPECT_EQ(tx->state_history[0]->state, ITS::kStarted);
  EXPECT_EQ(tx->state_history[0]->duration, 10);
  EXPECT_EQ(tx->state_history[1]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[1]->duration, 10);
  EXPECT_EQ(tx->state_history[2]->state, ITS::kCommitting);
  EXPECT_EQ(tx->state_history[2]->duration, 10);
  EXPECT_EQ(tx->state_history[3]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[3]->duration, 0);

  // Snapshot 5.
  tx = std::move(result[5]->databases[0]->transactions[0]);
  EXPECT_THAT(tx->state_history, SizeIs(5));
  EXPECT_EQ(tx->state_history[0]->state, ITS::kStarted);
  EXPECT_EQ(tx->state_history[0]->duration, 10);
  EXPECT_EQ(tx->state_history[1]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[1]->duration, 10);
  EXPECT_EQ(tx->state_history[2]->state, ITS::kCommitting);
  EXPECT_EQ(tx->state_history[2]->duration, 10);
  EXPECT_EQ(tx->state_history[3]->state, ITS::kRunning);
  EXPECT_EQ(tx->state_history[3]->duration, 20);
  EXPECT_EQ(tx->state_history[4]->state, ITS::kFinished);
  EXPECT_EQ(tx->state_history[4]->duration, 0);
}

}  // namespace content::indexed_db
