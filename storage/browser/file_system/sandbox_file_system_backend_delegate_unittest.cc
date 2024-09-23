// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

FileSystemURL CreateFileSystemURL(const char* path) {
  return FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo/"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class SandboxFileSystemBackendDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_proxy_ = base::MakeRefCounted<MockQuotaManagerProxy>(
        nullptr, base::SingleThreadTaskRunner::GetCurrentDefault());
    delegate_ = std::make_unique<SandboxFileSystemBackendDelegate>(
        quota_manager_proxy_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        data_dir_.GetPath(), /*special_storage_policy=*/nullptr,
        CreateAllowFileAccessOptions(), /*env_override=*/nullptr);
  }

  bool IsAccessValid(const FileSystemURL& url) const {
    return delegate_->IsAccessValid(url);
  }

  void OpenFileSystem(const BucketLocator& bucket_locator,
                      FileSystemType type,
                      OpenFileSystemMode mode) {
    delegate_->OpenFileSystem(
        bucket_locator, type, mode,
        base::BindOnce(
            &SandboxFileSystemBackendDelegateTest::OpenFileSystemCallback,
            base::Unretained(this)),
        GURL());
    task_environment_.RunUntilIdle();
  }

  int callback_count() const { return callback_count_; }

  base::File::Error last_error() const { return last_error_; }

  MockQuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

 private:
  void OpenFileSystemCallback(const GURL& root_url,
                              const std::string& name,
                              base::File::Error error) {
    ++callback_count_;
    last_error_ = error;
  }

  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<SandboxFileSystemBackendDelegate> delegate_;

  int callback_count_ = 0;
  base::File::Error last_error_ = base::File::FILE_OK;
};

TEST_F(SandboxFileSystemBackendDelegateTest, IsAccessValid) {
  // Normal case.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("a")));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("a/../b")));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_FALSE(IsAccessValid(FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("unknown://bar"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("foo"))));

  // Access with restricted name should be disallowed.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(".")));
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("..")));

  // This is also disallowed due to Windows XP parent path handling.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("...")));

  // These are identified as unsafe cases due to weird path handling
  // on Windows.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(" ..")));
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(".. ")));

  // Similar but safe cases.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(" .")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(". ")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("b.")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(".b")));

  // A path that looks like a drive letter.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("c:")));
}

TEST_F(SandboxFileSystemBackendDelegateTest, OpenFileSystemAccessesStorage) {
  EXPECT_EQ(quota_manager_proxy()->notify_bucket_accessed_count(), 0);
  EXPECT_EQ(callback_count(), 0);

  const blink::StorageKey& storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");

  // TODO(crbug.com/40227222): ensure that this test suite properly
  // integrates non-default BucketLocators into OpenFileSystem.
  OpenFileSystem(BucketLocator::ForDefaultBucket(storage_key),
                 kFileSystemTypeTemporary,
                 OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);

  EXPECT_EQ(callback_count(), 1);
  EXPECT_EQ(last_error(), base::File::FILE_OK);
  EXPECT_EQ(quota_manager_proxy()->notify_bucket_accessed_count(), 1);
  EXPECT_EQ(quota_manager_proxy()->last_notified_storage_key(), storage_key);
  EXPECT_EQ(quota_manager_proxy()->last_notified_type(),
            blink::mojom::StorageType::kTemporary);
}

}  // namespace storage
