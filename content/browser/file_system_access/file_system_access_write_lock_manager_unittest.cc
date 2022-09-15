// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_write_lock_manager.h"

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

  void AssertAncestorLockBehavior(const FileSystemURL& parent_url,
                                  const FileSystemURL& child_url) {
    // Parent cannot take exclusive lock if child is exclusively locked.
    {
      auto child_lock =
          manager_->TakeWriteLock(child_url, WriteLockType::kExclusive);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(
          manager_->TakeWriteLock(parent_url, WriteLockType::kExclusive));
    }

    // Parent can take shared lock if child is exclusively locked.
    {
      auto child_lock =
          manager_->TakeWriteLock(child_url, WriteLockType::kExclusive);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(manager_->TakeWriteLock(parent_url, WriteLockType::kShared));
    }

    // Child cannot take exclusive lock if parent is exclusively locked.
    {
      auto parent_lock =
          manager_->TakeWriteLock(parent_url, WriteLockType::kExclusive);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(
          manager_->TakeWriteLock(child_url, WriteLockType::kExclusive));
    }

    // Child cannot take shared lock if parent is exclusively locked.
    {
      auto parent_lock =
          manager_->TakeWriteLock(parent_url, WriteLockType::kExclusive);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(manager_->TakeWriteLock(child_url, WriteLockType::kShared));
    }

    // Parent cannot take exclusive lock if child holds a shared lock.
    {
      auto child_lock =
          manager_->TakeWriteLock(child_url, WriteLockType::kShared);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(
          manager_->TakeWriteLock(parent_url, WriteLockType::kExclusive));
    }

    // Parent can take shared lock if child holds a shared lock.
    {
      auto child_lock =
          manager_->TakeWriteLock(child_url, WriteLockType::kShared);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(manager_->TakeWriteLock(parent_url, WriteLockType::kShared));
    }

    // Child can take exclusive lock if parent holds a shared lock.
    {
      auto parent_lock =
          manager_->TakeWriteLock(parent_url, WriteLockType::kShared);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(
          manager_->TakeWriteLock(child_url, WriteLockType::kExclusive));
    }

    // Child can take shared lock if parent holds a shared lock.
    {
      auto parent_lock =
          manager_->TakeWriteLock(parent_url, WriteLockType::kShared);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(manager_->TakeWriteLock(child_url, WriteLockType::kShared));
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

TEST_F(FileSystemAccessWriteLockManagerTest, ExclusiveLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  {
    auto exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
    ASSERT_FALSE(manager_->TakeWriteLock(url, WriteLockType::kShared));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
}

TEST_F(FileSystemAccessWriteLockManagerTest, SharedLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  {
    auto shared_lock = manager_->TakeWriteLock(url, WriteLockType::kShared);
    ASSERT_TRUE(shared_lock);

    // Can take another shared lock, but not an exclusive lock.
    ASSERT_TRUE(manager_->TakeWriteLock(url, WriteLockType::kShared));
    ASSERT_FALSE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
  }

  // The shared locks have been released and we should be available to acquire
  // an exclusive lock.
  ASSERT_TRUE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
}

TEST_F(FileSystemAccessWriteLockManagerTest, SandboxedFile) {
  auto url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  url.SetBucket(kTestBucketLocator);

  {
    auto exclusive_lock =
        manager_->TakeWriteLock(url, WriteLockType::kExclusive);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
    ASSERT_FALSE(manager_->TakeWriteLock(url, WriteLockType::kShared));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeWriteLock(url, WriteLockType::kExclusive));
}

TEST_F(FileSystemAccessWriteLockManagerTest, SandboxedFilesSamePath) {
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

  // Take a lock on the file in the first file system.
  auto exclusive_lock1 =
      manager_->TakeWriteLock(url1, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(manager_->TakeWriteLock(url1, WriteLockType::kExclusive));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 =
      manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(manager_->TakeWriteLock(url2, WriteLockType::kExclusive));
}

TEST_F(FileSystemAccessWriteLockManagerTest, SandboxedFilesDifferentBucket) {
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

  // Take a lock on the file in the first file system.
  auto exclusive_lock1 =
      manager_->TakeWriteLock(url1, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(manager_->TakeWriteLock(url1, WriteLockType::kExclusive));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 =
      manager_->TakeWriteLock(url2, WriteLockType::kExclusive);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(manager_->TakeWriteLock(url2, WriteLockType::kExclusive));
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
  ASSERT_TRUE(local_exclusive_lock);
  ASSERT_FALSE(manager_->TakeWriteLock(local_url, WriteLockType::kExclusive));

  // Can still take a lock on the file in the external file system.
  auto external_exclusive_lock =
      manager_->TakeWriteLock(external_url, WriteLockType::kExclusive);
  ASSERT_TRUE(external_exclusive_lock);
  ASSERT_FALSE(
      manager_->TakeWriteLock(external_url, WriteLockType::kExclusive));
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
    ASSERT_TRUE(exclusive_lock);

    // Other sites cannot access the file while it is exclusively locked.
    ASSERT_FALSE(manager_->TakeWriteLock(url2, WriteLockType::kExclusive));
    ASSERT_FALSE(manager_->TakeWriteLock(url2, WriteLockType::kShared));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(manager_->TakeWriteLock(url2, WriteLockType::kExclusive));
}

TEST_F(FileSystemAccessWriteLockManagerTest, AncestorLocks) {
  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessWriteLockManagerTest, AncestorLocksExternal) {
  base::FilePath parent_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessWriteLockManagerTest, AncestorLocksSandboxed) {
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
