// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_manager_host.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "https://www.google.com";

}  // namespace

class BucketManagerHostTest : public testing::Test {
 public:
  BucketManagerHostTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}
  ~BucketManagerHostTest() override = default;

  BucketManagerHostTest(const BucketManagerHostTest&) = delete;
  BucketManagerHostTest& operator=(const BucketManagerHostTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get(), special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get());
    bucket_manager_ =
        std::make_unique<BucketManager>(quota_manager_proxy_.get());
    bucket_manager_->BindReceiver(
        url::Origin::Create(GURL(kTestUrl)),
        bucket_manager_host_remote_.BindNewPipeAndPassReceiver(),
        base::DoNothing());
    EXPECT_TRUE(bucket_manager_host_remote_.is_bound());
  }

 protected:
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because it uses the thread pool for
  // querying QuotaDatabase
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<blink::mojom::BucketManagerHost> bucket_manager_host_remote_;
  std::unique_ptr<BucketManager> bucket_manager_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
};

TEST_F(BucketManagerHostTest, OpenBucket) {
  base::RunLoop run_loop;
  bucket_manager_host_remote_->OpenBucket(
      "inbox_bucket", blink::mojom::BucketPolicies::New(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::BucketHost> remote) {
            EXPECT_TRUE(remote.is_valid());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Check that bucket is in QuotaDatabase.
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager_->GetBucket(
      blink::StorageKey::CreateFromStringForTesting(kTestUrl), "inbox_bucket",
      blink::mojom::StorageType::kTemporary, bucket_future.GetCallback());
  auto result = bucket_future.Take();
  EXPECT_TRUE(result.ok());
  EXPECT_GT(result->id.value(), 0u);
}

TEST_F(BucketManagerHostTest, OpenBucketValidateName) {
  const std::vector<std::pair</*is_valid=*/bool, std::string>> names = {
      {false, ""},
      {false, " "},
      {false, "2021/01/01"},
      {false, "_bucket_with_underscore_start"},
      {false, "-bucket-with-dash-start"},
      {false, "UpperCaseBucketName"},
      {false,
       "a_bucket_name_that_is_too_long_and_exceeds_sixty_four_characters_"},
      {true, "1"},
      {true, "bucket_with_underscore"},
      {true, "bucket-with-dash"},
      {true, "2021_01_01"},
      {true, "2021-01-01"},
      {true, "a_bucket_name_under_sixty_four_characters"}};

  for (auto it = names.begin(); it < names.end(); ++it) {
    mojo::Remote<blink::mojom::BucketManagerHost> remote;
    bucket_manager_->BindReceiver(url::Origin::Create(GURL(kTestUrl)),
                                  remote.BindNewPipeAndPassReceiver(),
                                  base::DoNothing());
    EXPECT_TRUE(remote.is_bound());

    if (it->first) {
      base::RunLoop run_loop;
      remote->OpenBucket(
          it->second, blink::mojom::BucketPolicies::New(),
          base::BindLambdaForTesting(
              [&](mojo::PendingRemote<blink::mojom::BucketHost> remote) {
                EXPECT_EQ(remote.is_valid(), it->first);
                run_loop.Quit();
              }));
      run_loop.Run();
    } else {
      mojo::test::BadMessageObserver bad_message_observer;
      remote->OpenBucket(it->second, blink::mojom::BucketPolicies::New(),
                         blink::mojom::BucketManagerHost::OpenBucketCallback());
      remote.FlushForTesting();
      EXPECT_EQ("Invalid bucket name",
                bad_message_observer.WaitForBadMessage());
    }
  }
}

TEST_F(BucketManagerHostTest, DeleteBucket) {
  base::RunLoop run_loop;
  bucket_manager_host_remote_->OpenBucket(
      "inbox_bucket", blink::mojom::BucketPolicies::New(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::BucketHost> remote) {
            EXPECT_TRUE(remote.is_valid());
            run_loop.Quit();
          }));
  run_loop.Run();

  base::test::TestFuture<bool> delete_future;
  bucket_manager_host_remote_->DeleteBucket("inbox_bucket",
                                            delete_future.GetCallback());
  bool deleted = delete_future.Get();
  EXPECT_TRUE(deleted);

  // Check that bucket is not in QuotaDatabase.
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager_->GetBucket(
      blink::StorageKey::CreateFromStringForTesting(kTestUrl), "inbox_bucket",
      blink::mojom::StorageType::kTemporary, bucket_future.GetCallback());
  auto result = bucket_future.Take();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), storage::QuotaError::kNotFound);
}

TEST_F(BucketManagerHostTest, DeleteInvalidBucketName) {
  mojo::test::BadMessageObserver bad_message_observer;
  bucket_manager_host_remote_->DeleteBucket("InvalidBucket", base::DoNothing());
  bucket_manager_host_remote_.FlushForTesting();
  EXPECT_EQ("Invalid bucket name", bad_message_observer.WaitForBadMessage());
}

}  // namespace content
