// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

class QuotaManagerProxyTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_TRUE(profile_path_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<QuotaManagerImpl>(
        /*is_incognito*/ false, profile_path_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*storage_policy=*/nullptr,
        base::BindRepeating([](OptionalQuotaSettingsCallback callback) {
          QuotaSettings settings;
          settings.per_storage_key_quota = 200 * 1024 * 1024;
          std::move(callback).Run(settings);
        }));
    quota_manager_proxy_ = quota_manager_->proxy();
  }

  void TearDown() override {
    quota_manager_proxy_ = nullptr;
    quota_manager_ = nullptr;
  }

 protected:
  base::ScopedTempDir profile_path_;
  scoped_refptr<QuotaManagerImpl> quota_manager_;
  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(QuotaManagerProxyTest, GetBucketPath) {
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
  BucketInitParams params(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      "draft_bucket");
  quota_manager_proxy_->UpdateOrCreateBucket(
      params, base::SingleThreadTaskRunner::GetCurrentDefault(),
      future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket, future.Take());

  base::FilePath expected_path =
      profile_path_.GetPath()
          .AppendASCII("WebStorage")
          .AppendASCII(base::NumberToString(bucket.id.value()));
  EXPECT_EQ(quota_manager_proxy_->GetBucketPath(bucket.ToBucketLocator()),
            expected_path);
}

TEST_F(QuotaManagerProxyTest, GetClientBucketPath) {
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
  BucketInitParams params(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      "draft_bucket");
  quota_manager_proxy_->UpdateOrCreateBucket(
      params, base::SingleThreadTaskRunner::GetCurrentDefault(),
      future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket, future.Take());

  base::FilePath bucket_path =
      profile_path_.GetPath()
          .AppendASCII("WebStorage")
          .AppendASCII(base::NumberToString(bucket.id.value()));

  // FileSystem
  base::FilePath expected_path = bucket_path.AppendASCII("FileSystem");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket.ToBucketLocator(), QuotaClientType::kFileSystem),
            expected_path);

  // IndexedDb
  expected_path = bucket_path.AppendASCII("IndexedDB");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket.ToBucketLocator(), QuotaClientType::kIndexedDatabase),
            expected_path);

  // BackgroundFetch
  expected_path = bucket_path.AppendASCII("BackgroundFetch");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket.ToBucketLocator(), QuotaClientType::kBackgroundFetch),
            expected_path);

  // CacheStorage
  expected_path = bucket_path.AppendASCII("CacheStorage");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket.ToBucketLocator(), QuotaClientType::kServiceWorkerCache),
            expected_path);

  // ServiceWorker
  expected_path = bucket_path.AppendASCII("ScriptCache");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket.ToBucketLocator(), QuotaClientType::kServiceWorker),
            expected_path);
}

TEST_F(QuotaManagerProxyTest, GetBucketsModifiedBetween) {
  base::test::TestFuture<QuotaErrorOr<BucketInfo>> create_future;
  BucketInitParams params(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      "draft_bucket");
  quota_manager_proxy_->UpdateOrCreateBucket(
      params, base::SingleThreadTaskRunner::GetCurrentDefault(),
      create_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket, create_future.Take());

  base::test::TestFuture<std::set<BucketLocator>> buckets_future;
  quota_manager_proxy_->GetBucketsModifiedBetween(
      base::Time(), base::Time::Max(),
      buckets_future.GetCallback<const std::set<BucketLocator>&>());

  EXPECT_EQ(std::set<BucketLocator>({bucket.ToBucketLocator()}),
            buckets_future.Get());
}

TEST_F(QuotaManagerProxyTest, ManagerDestructionInitiatedDuringPostTask) {
  // Queue a `PostTaskWithQuotaManager` task.
  base::test::TestFuture<base::WeakPtr<QuotaManagerImpl>> future;
  quota_manager_proxy_->PostTaskWithQuotaManager(future.GetCallback());

  // Release the last reference on a different thread, before the previously
  // posted task executes.
  base::Thread release_thread("QuotaManagerReleaser");
  ASSERT_TRUE(release_thread.Start());
  release_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<QuotaManagerImpl> manager) { manager.reset(); },
          std::move(quota_manager_)));
  release_thread.FlushForTesting();

  // The callback is invoked with a valid WeakPtr to the manager. (We could have
  // invoked it with null in this case, but in the general case we cannot, since
  // checking the existence of a reference is racy).
  base::WeakPtr<QuotaManagerImpl> manager = future.Take();
  ASSERT_TRUE(manager);
  EXPECT_FALSE(manager->HasAtLeastOneRef());

  // The manager deletes itself on the `io_sequence` that was passed to it at
  // construction time, which is the current sequence.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(manager);
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      quota_manager_proxy_->quota_manager_impl_sequence_checker_);
  EXPECT_FALSE(quota_manager_proxy_->quota_manager_impl_);
}

}  // namespace storage
