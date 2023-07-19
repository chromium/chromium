// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using Lock = FileSystemAccessLockManager::Lock;
using LockType = FileSystemAccessLockManager::LockType;
using storage::FileSystemURL;

static constexpr char kTestMountPoint[] = "testfs";

class FileSystemAccessLockManagerTest : public testing::Test {
 public:
  FileSystemAccessLockManagerTest()
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

  void AssertAncestorLockBehavior(const FileSystemURL& parent_url,
                                  const FileSystemURL& child_url) {
    LockType exclusive_lock_type = manager_->GetExclusiveLockType();
    LockType ancestor_lock_type = manager_->GetAncestorLockTypeForTesting();
    LockType shared_lock_type = manager_->CreateSharedLockType();
    // Parent cannot take an exclusive lock if child holds an exclusive lock.
    {
      auto child_lock = manager_->TakeLock(child_url, exclusive_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(manager_->TakeLock(parent_url, exclusive_lock_type));
    }

    // Parent can take an ancestor lock if child holds an exclusive lock.
    {
      auto child_lock = manager_->TakeLock(child_url, exclusive_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(manager_->TakeLock(parent_url, ancestor_lock_type));
    }

    // Child cannot take an exclusive lock if parent holds an exclusive lock.
    {
      auto parent_lock = manager_->TakeLock(parent_url, exclusive_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(manager_->TakeLock(child_url, exclusive_lock_type));
    }

    // Child can take an exclusive lock if parent holds an ancestor lock.
    {
      auto parent_lock = manager_->TakeLock(parent_url, ancestor_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(manager_->TakeLock(child_url, exclusive_lock_type));
    }

    // Parent cannot take an exclusive lock if child holds a shared lock.
    {
      auto child_lock = manager_->TakeLock(child_url, shared_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(manager_->TakeLock(parent_url, exclusive_lock_type));
    }

    // Parent can take an ancestor lock if child holds a shared lock.
    {
      auto child_lock = manager_->TakeLock(child_url, shared_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(manager_->TakeLock(parent_url, ancestor_lock_type));
    }

    // Child cannot take a shared lock if parent holds an exclusive lock.
    {
      auto parent_lock = manager_->TakeLock(parent_url, exclusive_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(manager_->TakeLock(child_url, shared_lock_type));
    }

    // Child can take a shared lock if parent holds an ancestor lock.
    {
      auto parent_lock = manager_->TakeLock(parent_url, ancestor_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(manager_->TakeLock(child_url, shared_lock_type));
    }
  }

 protected:
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const storage::BucketLocator kTestBucketLocator =
      storage::BucketLocator(storage::BucketId(1),
                             kTestStorageKey,
                             blink::mojom::StorageType::kTemporary,
                             /*is_default=*/false);

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
};

TEST_F(FileSystemAccessLockManagerTest, ExclusiveLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockType();
  {
    auto exclusive_lock = manager_->TakeLock(url, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(manager_->TakeLock(url, exclusive_lock_type));
    ASSERT_FALSE(manager_->TakeLock(url, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeLock(url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SharedLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type_1 = manager_->CreateSharedLockType();
  LockType shared_lock_type_2 = manager_->CreateSharedLockType();
  {
    auto shared_lock = manager_->TakeLock(url, shared_lock_type_1);
    ASSERT_TRUE(shared_lock);

    // Can take another shared lock of the same type, but not an exclusive lock
    // or a shared lock of another type.
    ASSERT_TRUE(manager_->TakeLock(url, shared_lock_type_1));
    ASSERT_FALSE(manager_->TakeLock(url, exclusive_lock_type));
    ASSERT_FALSE(manager_->TakeLock(url, shared_lock_type_2));
  }

  // The shared locks have been released and we should be available to acquire
  // an exclusive lock.
  ASSERT_TRUE(manager_->TakeLock(url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SandboxedFile) {
  auto url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  url.SetBucket(kTestBucketLocator);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockType();
  {
    auto exclusive_lock = manager_->TakeLock(url, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(manager_->TakeLock(url, exclusive_lock_type));
    ASSERT_FALSE(manager_->TakeLock(url, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeLock(url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SandboxedFilesSamePath) {
  // Sandboxed files of the same relative path do not lock across sites if the
  // BucketLocator is set.
  const blink::StorageKey kOtherStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://foo.com/test");
  const auto path = base::FilePath::FromUTF8Unsafe("test/foo/bar");
  auto url1 = file_system_context_->CreateCrackedFileSystemURL(
      kOtherStorageKey, storage::kFileSystemTypeTemporary, path);
  url1.SetBucket(kTestBucketLocator);
  auto url2 = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, path);
  const storage::BucketLocator kOtherBucketLocator(
      storage::BucketId(2), kOtherStorageKey,
      blink::mojom::StorageType::kTemporary,
      /*is_default=*/false);
  url2.SetBucket(kOtherBucketLocator);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  // Take a lock on the file in the first file system.
  auto exclusive_lock1 = manager_->TakeLock(url1, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(manager_->TakeLock(url1, exclusive_lock_type));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 = manager_->TakeLock(url2, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(manager_->TakeLock(url2, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SandboxedFilesDifferentBucket) {
  // Sandboxed files of the same relative path do not lock across buckets.
  const auto path = base::FilePath::FromUTF8Unsafe("test/foo/bar");
  auto url1 = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, path);
  url1.SetBucket(kTestBucketLocator);
  auto url2 = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, path);
  const storage::BucketLocator kOtherBucketLocator(
      storage::BucketId(2), kTestStorageKey,
      blink::mojom::StorageType::kTemporary,
      /*is_default=*/false);
  url2.SetBucket(kOtherBucketLocator);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  // Take a lock on the file in the first file system.
  auto exclusive_lock1 = manager_->TakeLock(url1, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(manager_->TakeLock(url1, exclusive_lock_type));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 = manager_->TakeLock(url2, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(manager_->TakeLock(url2, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, DifferentBackends) {
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

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  // Take a lock on the file in the local file system.
  auto local_exclusive_lock =
      manager_->TakeLock(local_url, exclusive_lock_type);
  ASSERT_TRUE(local_exclusive_lock);
  ASSERT_FALSE(manager_->TakeLock(local_url, exclusive_lock_type));

  // Can still take a lock on the file in the external file system.
  auto external_exclusive_lock =
      manager_->TakeLock(external_url, exclusive_lock_type);
  ASSERT_TRUE(external_exclusive_lock);
  ASSERT_FALSE(manager_->TakeLock(external_url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, LockAcrossSites) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url1 = FileSystemURL::CreateForTest(kTestStorageKey,
                                           storage::kFileSystemTypeLocal, path);

  // Select the same local file from another site.
  auto url2 = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("https://foo.com/bar"),
      storage::kFileSystemTypeLocal, path);

  EXPECT_EQ(url1.path(), url2.path());
  EXPECT_NE(url1.storage_key(), url2.storage_key());

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockType();

  {
    auto exclusive_lock = manager_->TakeLock(url1, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Other sites cannot access the file while it is exclusively locked.
    ASSERT_FALSE(manager_->TakeLock(url2, exclusive_lock_type));
    ASSERT_FALSE(manager_->TakeLock(url2, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeLock(url2, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocks) {
  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocksExternal) {
  base::FilePath parent_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocksSandboxed) {
  auto parent_path = base::FilePath::FromUTF8Unsafe("test/foo/bar");
  auto parent_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, parent_path);
  parent_url.SetBucket(kTestBucketLocator);
  auto child_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      parent_path.Append(FILE_PATH_LITERAL("child")));
  child_url.SetBucket(kTestBucketLocator);

  AssertAncestorLockBehavior(parent_url, child_url);
}

}  // namespace content
