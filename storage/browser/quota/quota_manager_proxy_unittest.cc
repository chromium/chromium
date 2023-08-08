// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*quota_change_callback=*/base::DoNothing(),
        /*storage_policy=*/nullptr,
        base::BindRepeating([](OptionalQuotaSettingsCallback callback) {
          QuotaSettings settings;
          settings.per_storage_key_quota = 200 * 1024 * 1024;
          std::move(callback).Run(settings);
        }));
    quota_manager_proxy_ = base::MakeRefCounted<QuotaManagerProxy>(
        quota_manager_.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
        profile_path_.GetPath());
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

}  // namespace storage
