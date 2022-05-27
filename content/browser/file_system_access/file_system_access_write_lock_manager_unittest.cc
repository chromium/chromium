// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_write_lock_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

using WriteLock = FileSystemAccessWriteLockManager::WriteLock;
using WriteLockType = FileSystemAccessWriteLockManager::WriteLockType;
using storage::FileSystemURL;

static constexpr char kTestMountPoint[] = "testfs";

class FileSystemAccessWriteLockManagerTest : public testing::Test {
 public:
  FileSystemAccessWriteLockManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    // Register an external mount point to test support for virtual paths.
    // This maps the virtual path a native local path to make these tests work
    // on all platforms. We're not testing more complicated ChromeOS specific
    // file system backends here.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kTestMountPoint, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), dir_.GetPath());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

  void TearDown() override {
    manager_.reset();

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(dir_.Delete());
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const int kProcessId = 1;
  const int kFrameRoutingId = 2;
  const GlobalRenderFrameHostId kFrameId{kProcessId, kFrameRoutingId};
  const FileSystemAccessManagerImpl::BindingContext kBindingContext = {
      kTestStorageKey, kTestURL, kFrameId};

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
};

TEST_F(FileSystemAccessWriteLockManagerTest, ExclusiveLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  {
    auto exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_TRUE(exclusive_lock.has_value());

    // Cannot take another lock while the file is exclusively locked.
    auto another_exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_FALSE(another_exclusive_lock.has_value());
    auto shared_lock = manager_->TakeWriteLock(url, WriteLockType::kShared);
    ASSERT_FALSE(shared_lock.has_value());
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  auto exclusive_lock = manager_->TakeWriteLock(url, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock.has_value());
}

TEST_F(FileSystemAccessWriteLockManagerTest, SharedLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  {
    auto shared_lock = manager_->TakeWriteLock(url, WriteLockType::kShared);
    ASSERT_TRUE(shared_lock.has_value());

    // Can take another shared lock, but not an exclusive lock.
    auto another_shared_lock =
        manager_->TakeWriteLock(url, WriteLockType::kShared);
    ASSERT_TRUE(another_shared_lock.has_value());
    auto exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_FALSE(exclusive_lock.has_value());
  }

  // The shared locks have been released and we should be available to acquire
  // an exclusive lock.
  auto exclusive_lock = manager_->TakeWriteLock(url, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock.has_value());
}

TEST_F(FileSystemAccessWriteLockManagerTest, SandboxedFile) {
  auto url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));

  {
    auto exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_TRUE(exclusive_lock.has_value());

    // Cannot take another lock while the file is exclusively locked.
    auto another_exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_FALSE(another_exclusive_lock.has_value());
    auto shared_lock = manager_->TakeWriteLock(url, WriteLockType::kShared);
    ASSERT_FALSE(shared_lock.has_value());
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  auto exclusive_lock = manager_->TakeWriteLock(url, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock.has_value());
}

TEST_F(FileSystemAccessWriteLockManagerTest, SandboxedFilesSamePath) {
  // Sandboxed files of the same relative path do not lock across sites if the
  // BucketLocator is set.
  const blink::StorageKey kOtherStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://foo.com/test");
  auto url1 = file_system_context_->CreateCrackedFileSystemURL(
      kOtherStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  auto url2 = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));

  // Take a lock on the file in the first file system.
  auto exclusive_lock1 =
      manager_->TakeWriteLock(url1, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock1.has_value());
  auto another_exclusive_lock1 =
      manager_->TakeWriteLock(url1, WriteLockType::kExclusive);
  ASSERT_FALSE(another_exclusive_lock1.has_value());

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 =
      manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock2.has_value());
  auto another_exclusive_lock2 =
      manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
  ASSERT_FALSE(another_exclusive_lock2.has_value());
}

TEST_F(FileSystemAccessWriteLockManagerTest, DifferentBackends) {
  // We'll use the same path and pretend they're from different backends.
  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");

  // File on a local file system.
  auto local_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  // File with the same path on an external file system.
  auto external_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal, path);

  EXPECT_EQ(local_url.path(), external_url.virtual_path());

  // Take a lock on the file in the local file system.
  auto local_exclusive_lock =
      manager_->TakeWriteLock(local_url, WriteLockType::kExclusive);
  ASSERT_TRUE(local_exclusive_lock.has_value());
  auto another_local_exclusive_lock =
      manager_->TakeWriteLock(local_url, WriteLockType::kExclusive);
  ASSERT_FALSE(another_local_exclusive_lock.has_value());

  // Can still take a lock on the file in the external file system.
  auto external_exclusive_lock =
      manager_->TakeWriteLock(external_url, WriteLockType::kExclusive);
  ASSERT_TRUE(external_exclusive_lock.has_value());
  auto another_external_exclusive_lock =
      manager_->TakeWriteLock(external_url, WriteLockType::kExclusive);
  ASSERT_FALSE(another_external_exclusive_lock.has_value());
}

TEST_F(FileSystemAccessWriteLockManagerTest, LockAcrossSites) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url1 = FileSystemURL::CreateForTest(kTestStorageKey,
                                           storage::kFileSystemTypeLocal, path);

  // Select the same local file from another site.
  auto url2 = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("https://foo.com/bar"),
      storage::kFileSystemTypeLocal, path);

  EXPECT_EQ(url1.path(), url2.path());
  EXPECT_NE(url1.storage_key(), url2.storage_key());

  {
    auto exclusive_lock =
        manager_->TakeWriteLock(url1, WriteLockType::kExclusive);
    ASSERT_TRUE(exclusive_lock.has_value());

    // Other sites cannot access the file while it is exclusively locked.
    auto another_exclusive_lock =
        manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
    ASSERT_FALSE(another_exclusive_lock.has_value());
    auto shared_lock = manager_->TakeWriteLock(url2, WriteLockType::kShared);
    ASSERT_FALSE(shared_lock.has_value());
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  auto exclusive_lock =
      manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock.has_value());
}

}  // namespace content
