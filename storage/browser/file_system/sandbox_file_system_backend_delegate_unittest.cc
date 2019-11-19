// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::FileSystemURL;

namespace content {

namespace {

FileSystemURL CreateFileSystemURL(const char* path) {
  const GURL kOrigin("http://foo/");
  return storage::FileSystemURL::CreateForTest(
      url::Origin::Create(kOrigin), storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class SandboxFileSystemBackendDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_proxy_ = new MockQuotaManagerProxy(
        nullptr, base::ThreadTaskRunnerHandle::Get().get());
    delegate_.reset(new storage::SandboxFileSystemBackendDelegate(
        quota_manager_proxy_.get(), base::ThreadTaskRunnerHandle::Get().get(),
        data_dir_.GetPath(), nullptr /* special_storage_policy */,
        CreateAllowFileAccessOptions(), nullptr /* env_override */));
  }

  bool IsAccessValid(const FileSystemURL& url) const {
    return delegate_->IsAccessValid(url);
  }

  void OpenFileSystem(const GURL& origin,
                      storage::FileSystemType type,
                      storage::OpenFileSystemMode mode) {
    delegate_->OpenFileSystem(
        origin, type, mode,
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
  std::unique_ptr<storage::SandboxFileSystemBackendDelegate> delegate_;

  int callback_count_ = 0;
  base::File::Error last_error_ = base::File::FILE_OK;
};

TEST_F(SandboxFileSystemBackendDelegateTest, IsAccessValid) {
  // Normal case.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("a")));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("a/../b")));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_FALSE(IsAccessValid(
      FileSystemURL::CreateForTest(url::Origin::Create(GURL("unknown://bar")),
                                   storage::kFileSystemTypeTemporary,
                                   base::FilePath::FromUTF8Unsafe("foo"))));

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
  GURL origin("http://example.com");

  EXPECT_EQ(quota_manager_proxy()->notify_storage_accessed_count(), 0);
  EXPECT_EQ(callback_count(), 0);

  OpenFileSystem(origin, storage::kFileSystemTypeTemporary,
                 storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);

  EXPECT_EQ(callback_count(), 1);
  EXPECT_EQ(last_error(), base::File::FILE_OK);
  EXPECT_EQ(quota_manager_proxy()->notify_storage_accessed_count(), 1);
  EXPECT_EQ(quota_manager_proxy()->last_notified_origin(),
            url::Origin::Create(origin));
  EXPECT_EQ(quota_manager_proxy()->last_notified_type(),
            blink::mojom::StorageType::kTemporary);
}

}  // namespace content
