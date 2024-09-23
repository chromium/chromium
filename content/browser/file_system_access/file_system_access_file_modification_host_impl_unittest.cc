// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_modification_host_impl.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Synchronous proxy to FileSystemAccessFileModificationHostImpl's
// RequestCapacityChange.
int64_t RequestCapacityChangeSync(
    FileSystemAccessFileModificationHostImpl* allocation_host,
    int64_t capacity_delta) {
  base::test::TestFuture<int64_t> future;
  allocation_host->RequestCapacityChange(capacity_delta, future.GetCallback());
  int64_t granted_capacity = future.Get();
  return granted_capacity;
}

}  // namespace

class FileSystemAccessFileModificationHostImplTest : public testing::Test {
 public:
  FileSystemAccessFileModificationHostImplTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(data_dir_.GetPath().IsAbsolute());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager(), base::SingleThreadTaskRunner::GetCurrentDefault());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy(), data_dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_, &permission_context_,
        /*off_the_record=*/false);

    auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe("test"));
    test_file_url.SetBucket(
        storage::BucketLocator::ForDefaultBucket(kTestStorageKey));
    mojo::Remote<blink::mojom::FileSystemAccessFileModificationHost>
        allocation_host_remote;
    allocation_host_ =
        std::make_unique<FileSystemAccessFileModificationHostImpl>(
            manager_.get(), test_file_url,
            base::PassKey<FileSystemAccessFileModificationHostImplTest>(),
            allocation_host_remote.BindNewPipeAndPassReceiver(), 0);
  }

  void TearDown() override {
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
  }

 protected:
  storage::MockQuotaManager* quota_manager() {
    return static_cast<storage::MockQuotaManager*>(quota_manager_.get());
  }

  storage::MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<storage::MockQuotaManagerProxy*>(
        quota_manager_proxy_.get());
  }
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");

  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;
  BrowserTaskEnvironment task_environment_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  std::unique_ptr<FileSystemAccessFileModificationHostImpl> allocation_host_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
};

TEST_F(FileSystemAccessFileModificationHostImplTest,
       RequestCapacityChange_PositiveCapacity) {
  const int64_t requested_capacity = 50;
  int64_t granted_capacity =
      RequestCapacityChangeSync(allocation_host_.get(), requested_capacity);
  EXPECT_EQ(granted_capacity, requested_capacity);
  EXPECT_EQ(*quota_manager_proxy_->last_notified_bucket_delta(),
            requested_capacity);
}

TEST_F(FileSystemAccessFileModificationHostImplTest,
       RequestCapacityChange_PositiveAndNegativeCapacity) {
  const int64_t positive_requested_capacity = 50;
  const int64_t negative_requested_capacity = -40;

  int64_t positive_granted_capacity = RequestCapacityChangeSync(
      allocation_host_.get(), positive_requested_capacity);
  EXPECT_EQ(positive_granted_capacity, positive_requested_capacity);
  EXPECT_EQ(*quota_manager_proxy_->last_notified_bucket_delta(),
            positive_requested_capacity);

  int64_t negative_granted_capacity = RequestCapacityChangeSync(
      allocation_host_.get(), negative_requested_capacity);
  EXPECT_EQ(negative_granted_capacity, negative_requested_capacity);
  EXPECT_EQ(*quota_manager_proxy_->last_notified_bucket_delta(),
            negative_requested_capacity);

  EXPECT_EQ(quota_manager_proxy_->notify_bucket_modified_count(), 2);
}

TEST_F(FileSystemAccessFileModificationHostImplTest,
       RequestCapacityChange_IllegalNegativeCapacity) {
  mojo::test::BadMessageObserver bad_message_observer;
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  const int64_t negative_requested_capacity = -50;
  int64_t granted_capacity = RequestCapacityChangeSync(
      allocation_host_.get(), negative_requested_capacity);
  EXPECT_EQ(granted_capacity, 0);
  EXPECT_EQ(quota_manager_proxy_->notify_bucket_modified_count(), 0);
  EXPECT_EQ("A file's size cannot be negative or out of bounds",
            bad_message_observer.WaitForBadMessage());
}
}  // namespace content
