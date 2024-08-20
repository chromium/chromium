// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_manager_host.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
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
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        browser_context_(std::make_unique<TestBrowserContext>()) {}
  ~BucketManagerHostTest() override = default;

  BucketManagerHostTest(const BucketManagerHostTest&) = delete;
  BucketManagerHostTest& operator=(const BucketManagerHostTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition());
    partition->OverrideQuotaManagerForTesting(quota_manager_.get());

    bucket_manager_ = std::make_unique<BucketManager>(partition);
    bucket_manager_->BindReceiver(
        test_bucket_context_.GetWeakPtr(),
        bucket_manager_host_remote_.BindNewPipeAndPassReceiver(),
        base::DoNothing());
    EXPECT_TRUE(bucket_manager_host_remote_.is_bound());
  }

  void OpenWithPolicies(blink::mojom::BucketPoliciesPtr policies) {
    base::RunLoop run_loop;
    bucket_manager_host_remote_->OpenBucket(
        "foo_bucket", std::move(policies),
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
                blink::mojom::BucketError error) {
              EXPECT_TRUE(remote.is_valid());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

 protected:
  class TestBucketContext : public BucketContext {
   public:
    TestBucketContext() = default;
    ~TestBucketContext() override = default;

    // BucketContext:
    blink::StorageKey GetBucketStorageKey() override {
      return blink::StorageKey::CreateFromStringForTesting(kTestUrl);
    }
    blink::mojom::PermissionStatus GetPermissionStatus(
        blink::PermissionType permission_type) override {
      return permission_status_;
    }
    void BindCacheStorageForBucket(
        const storage::BucketInfo& bucket,
        mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override {}
    storage::BucketClientInfo GetBucketClientInfo() const override {
      return storage::BucketClientInfo{};
    }

    void GetSandboxedFileSystemForBucket(
        const storage::BucketInfo& bucket,
        const std::vector<std::string>& directory_path_components,
        blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
            callback) override {
      std::move(callback).Run(file_system_access_error::Ok(), {});
    }

    void set_permission_status(
        blink::mojom::PermissionStatus permission_status) {
      permission_status_ = permission_status;
    }

    base::WeakPtr<TestBucketContext> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    blink::mojom::PermissionStatus permission_status_ =
        blink::mojom::PermissionStatus::DENIED;
    base::WeakPtrFactory<TestBucketContext> weak_ptr_factory_{this};
  };

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because they use the thread pool
  // for querying QuotaDatabase.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  mojo::Remote<blink::mojom::BucketManagerHost> bucket_manager_host_remote_;
  std::unique_ptr<BucketManager> bucket_manager_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  TestBucketContext test_bucket_context_;
};

TEST_F(BucketManagerHostTest, OpenBucket) {
  base::RunLoop run_loop;
  bucket_manager_host_remote_->OpenBucket(
      "inbox_bucket", blink::mojom::BucketPolicies::New(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
              blink::mojom::BucketError error) {
            EXPECT_TRUE(remote.is_valid());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Check that bucket is in QuotaDatabase.
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager_->GetBucketByNameUnsafe(
      blink::StorageKey::CreateFromStringForTesting(kTestUrl), "inbox_bucket",
      blink::mojom::StorageType::kTemporary, bucket_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto result, bucket_future.Take());
  EXPECT_GT(result.id.value(), 0u);
}

TEST_F(BucketManagerHostTest, OpenBucketValidateName) {
  const std::vector<std::pair</*is_valid=*/bool, std::string>> names = {
      // The default name should not be a valid user-provided bucket name.
      {false, storage::kDefaultBucketName},
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
    bucket_manager_->BindReceiver(test_bucket_context_.GetWeakPtr(),
                                  remote.BindNewPipeAndPassReceiver(),
                                  base::DoNothing());
    EXPECT_TRUE(remote.is_bound());

    if (it->first) {
      base::RunLoop run_loop;
      remote->OpenBucket(
          it->second, blink::mojom::BucketPolicies::New(),
          base::BindLambdaForTesting(
              [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
                  blink::mojom::BucketError error) {
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
          [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
              blink::mojom::BucketError error) {
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
  quota_manager_->GetBucketByNameUnsafe(
      blink::StorageKey::CreateFromStringForTesting(kTestUrl), "inbox_bucket",
      blink::mojom::StorageType::kTemporary, bucket_future.GetCallback());
  auto result = bucket_future.Take();
  EXPECT_THAT(result, base::test::ErrorIs(storage::QuotaError::kNotFound));
}

TEST_F(BucketManagerHostTest, DeleteInvalidBucketName) {
  mojo::test::BadMessageObserver bad_message_observer;
  bucket_manager_host_remote_->DeleteBucket("InvalidBucket", base::DoNothing());
  bucket_manager_host_remote_.FlushForTesting();
  EXPECT_EQ("Invalid bucket name", bad_message_observer.WaitForBadMessage());
}

TEST_F(BucketManagerHostTest, PermissionCheck) {
  const std::vector<std::pair<blink::mojom::PermissionStatus,
                              /*persist_request_granted=*/bool>>
      test_cases = {{blink::mojom::PermissionStatus::GRANTED, true},
                    {blink::mojom::PermissionStatus::DENIED, false}};

  for (auto test_case : test_cases) {
    test_bucket_context_.set_permission_status(test_case.first);
    bool persist_request_granted = test_case.second;
    {
      // Not initially persisted.
      mojo::Remote<blink::mojom::BucketHost> bucket_remote;
      {
        base::RunLoop run_loop;
        bucket_manager_host_remote_->OpenBucket(
            "foo", blink::mojom::BucketPolicies::New(),
            base::BindLambdaForTesting(
                [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
                    blink::mojom::BucketError error) {
                  EXPECT_TRUE(remote.is_valid());
                  bucket_remote.Bind(std::move(remote));
                  run_loop.Quit();
                }));
        run_loop.Run();
      }

      {
        base::RunLoop run_loop;
        bucket_remote->Persisted(
            base::BindLambdaForTesting([&](bool persisted, bool success) {
              EXPECT_FALSE(persisted);
              EXPECT_TRUE(success);
              run_loop.Quit();
            }));
        run_loop.Run();
      }

      // Changed to persisted.
      {
        base::RunLoop run_loop;
        bucket_remote->Persist(
            base::BindLambdaForTesting([&](bool persisted, bool success) {
              EXPECT_EQ(persisted, persist_request_granted);
              EXPECT_TRUE(success);
              run_loop.Quit();
            }));
        run_loop.Run();
      }
      {
        base::test::TestFuture<bool> delete_future;
        bucket_manager_host_remote_->DeleteBucket("foo",
                                                  delete_future.GetCallback());
        EXPECT_TRUE(delete_future.Get());
      }

      // Initially persisted.
      mojo::Remote<blink::mojom::BucketHost> bucket_remote2;
      {
        base::RunLoop run_loop;
        auto policies = blink::mojom::BucketPolicies::New();
        policies->has_persisted = true;
        policies->persisted = true;
        bucket_manager_host_remote_->OpenBucket(
            "foo", std::move(policies),
            base::BindLambdaForTesting(
                [&](mojo::PendingRemote<blink::mojom::BucketHost> remote,
                    blink::mojom::BucketError error) {
                  EXPECT_TRUE(remote.is_valid());
                  bucket_remote2.Bind(std::move(remote));
                  run_loop.Quit();
                }));
        run_loop.Run();
      }

      {
        base::RunLoop run_loop;
        bucket_remote2->Persisted(
            base::BindLambdaForTesting([&](bool persisted, bool success) {
              EXPECT_EQ(persisted, persist_request_granted);
              EXPECT_TRUE(success);
              run_loop.Quit();
            }));
        run_loop.Run();
      }
      {
        base::test::TestFuture<bool> delete_future;
        bucket_manager_host_remote_->DeleteBucket("foo",
                                                  delete_future.GetCallback());
        EXPECT_TRUE(delete_future.Get());
      }
    }
  }
}

TEST_F(BucketManagerHostTest, Metrics) {
  base::HistogramTester tester;
  // Base case.
  OpenWithPolicies(blink::mojom::BucketPolicies::New());
  tester.ExpectUniqueSample("Storage.Buckets.Parameters.Expiration", 0, 1);
  tester.ExpectUniqueSample("Storage.Buckets.Parameters.QuotaKb", 0, 1);
  tester.ExpectUniqueSample("Storage.Buckets.Parameters.Durability", 0, 1);
  tester.ExpectUniqueSample("Storage.Buckets.Parameters.Persisted", 0, 1);

  // One hour and one day get different buckets.
  EXPECT_EQ(
      1U, tester.GetAllSamples("Storage.Buckets.Parameters.Expiration").size());
  {
    auto policies = blink::mojom::BucketPolicies::New();
    policies->expires = base::Time::Now() + base::Hours(1);
    OpenWithPolicies(std::move(policies));
  }
  EXPECT_EQ(
      2U, tester.GetAllSamples("Storage.Buckets.Parameters.Expiration").size());
  {
    auto policies = blink::mojom::BucketPolicies::New();
    policies->expires = base::Time::Now() + base::Days(1);
    OpenWithPolicies(std::move(policies));
  }
  EXPECT_EQ(
      3U, tester.GetAllSamples("Storage.Buckets.Parameters.Expiration").size());
}

}  // namespace content
