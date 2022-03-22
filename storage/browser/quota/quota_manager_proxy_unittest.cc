// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_task_runner_handle.h"
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
        base::ThreadTaskRunnerHandle::Get().get(),
        /*quota_change_callback=*/base::DoNothing(),
        /*storage_policy=*/nullptr, GetQuotaSettingsFunc());
    quota_manager_proxy_ = base::MakeRefCounted<QuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get(),
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
  quota_manager_proxy_->GetOrCreateBucket(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      "draft_bucket", base::ThreadTaskRunnerHandle::Get(),
      future.GetCallback());
  auto bucket = future.Take();
  EXPECT_TRUE(bucket.ok());

  base::FilePath expected_path =
      profile_path_.GetPath()
          .AppendASCII("WebStorage")
          .AppendASCII(base::NumberToString(bucket->id.value()));
  EXPECT_EQ(quota_manager_proxy_->GetBucketPath(bucket->ToBucketLocator()),
            expected_path);
}

TEST_F(QuotaManagerProxyTest, GetClientBucketPath) {
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
  quota_manager_proxy_->GetOrCreateBucket(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      "draft_bucket", base::ThreadTaskRunnerHandle::Get(),
      future.GetCallback());
  auto bucket = future.Take();
  EXPECT_TRUE(bucket.ok());

  base::FilePath bucket_path =
      profile_path_.GetPath()
          .AppendASCII("WebStorage")
          .AppendASCII(base::NumberToString(bucket->id.value()));

  // FileSystem
  base::FilePath expected_path = bucket_path.AppendASCII("FileSystem");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket->ToBucketLocator(), QuotaClientType::kFileSystem),
            expected_path);

  // IndexedDb
  expected_path = bucket_path.AppendASCII("IndexedDB");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket->ToBucketLocator(), QuotaClientType::kIndexedDatabase),
            expected_path);

  // BackgroundFetch/CacheStorage
  expected_path = bucket_path.AppendASCII("CacheStorage");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket->ToBucketLocator(), QuotaClientType::kBackgroundFetch),
            expected_path);
  EXPECT_EQ(
      quota_manager_proxy_->GetClientBucketPath(
          bucket->ToBucketLocator(), QuotaClientType::kServiceWorkerCache),
      expected_path);

  // ServiceWorker
  expected_path = bucket_path.AppendASCII("ScriptCache");
  EXPECT_EQ(quota_manager_proxy_->GetClientBucketPath(
                bucket->ToBucketLocator(), QuotaClientType::kServiceWorker),
            expected_path);
}

}  // namespace storage
