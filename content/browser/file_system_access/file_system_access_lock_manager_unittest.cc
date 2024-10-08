// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

using LockType = FileSystemAccessLockManager::LockType;
using storage::FileSystemURL;

static constexpr char kTestMountPoint[] = "testfs";

class FileSystemAccessLockManagerTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

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

    task_environment()->RunUntilIdle();
    EXPECT_TRUE(dir_.Delete());

    chrome_blob_context_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  storage::FileSystemURL CreateLocalUrl(const base::FilePath& path) {
    return manager_->CreateFileSystemURLFromPath(PathInfo(path));
  }

  std::unique_ptr<base::test::TestFuture<
      scoped_refptr<FileSystemAccessLockManager::LockHandle>>>
  TakeLockAsync(
      const FileSystemAccessManagerImpl::BindingContext binding_context,
      const storage::FileSystemURL& url,
      FileSystemAccessLockManager::LockType lock_type) {
    auto future = std::make_unique<base::test::TestFuture<
        scoped_refptr<FileSystemAccessLockManager::LockHandle>>>();
    manager_->TakeLock(binding_context, url, lock_type, future->GetCallback());
    return future;
  }

  scoped_refptr<FileSystemAccessLockManager::LockHandle> TakeLockSync(
      const FileSystemAccessManagerImpl::BindingContext binding_context,
      const storage::FileSystemURL& url,
      FileSystemAccessLockManager::LockType lock_type) {
    return TakeLockAsync(binding_context, url, lock_type)->Take();
  }

  void AssertAncestorLockBehavior(const FileSystemURL& parent_url,
                                  const FileSystemURL& child_url) {
    LockType exclusive_lock_type = manager_->GetExclusiveLockType();
    LockType ancestor_lock_type = manager_->GetAncestorLockTypeForTesting();
    LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();
    // Parent cannot take an exclusive lock if child holds an exclusive lock.
    {
      auto child_lock =
          TakeLockSync(kBindingContext, child_url, exclusive_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(
          TakeLockSync(kBindingContext, parent_url, exclusive_lock_type));
    }

    // Parent can take an ancestor lock if child holds an exclusive lock.
    {
      auto child_lock =
          TakeLockSync(kBindingContext, child_url, exclusive_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(
          TakeLockSync(kBindingContext, parent_url, ancestor_lock_type));
    }

    // Child cannot take an exclusive lock if parent holds an exclusive lock.
    {
      auto parent_lock =
          TakeLockSync(kBindingContext, parent_url, exclusive_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(
          TakeLockSync(kBindingContext, child_url, exclusive_lock_type));
    }

    // Child can take an exclusive lock if parent holds an ancestor lock.
    {
      auto parent_lock =
          TakeLockSync(kBindingContext, parent_url, ancestor_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(
          TakeLockSync(kBindingContext, child_url, exclusive_lock_type));
    }

    // Parent cannot take an exclusive lock if child holds a shared lock.
    {
      auto child_lock =
          TakeLockSync(kBindingContext, child_url, shared_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_FALSE(
          TakeLockSync(kBindingContext, parent_url, exclusive_lock_type));
    }

    // Parent can take an ancestor lock if child holds a shared lock.
    {
      auto child_lock =
          TakeLockSync(kBindingContext, child_url, shared_lock_type);
      ASSERT_TRUE(child_lock);
      ASSERT_TRUE(
          TakeLockSync(kBindingContext, parent_url, ancestor_lock_type));
    }

    // Child cannot take a shared lock if parent holds an exclusive lock.
    {
      auto parent_lock =
          TakeLockSync(kBindingContext, parent_url, exclusive_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_FALSE(TakeLockSync(kBindingContext, child_url, shared_lock_type));
    }

    // Child can take a shared lock if parent holds an ancestor lock.
    {
      auto parent_lock =
          TakeLockSync(kBindingContext, parent_url, ancestor_lock_type);
      ASSERT_TRUE(parent_lock);
      ASSERT_TRUE(TakeLockSync(kBindingContext, child_url, shared_lock_type));
    }
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const storage::BucketLocator kTestBucketLocator =
      storage::BucketLocator(storage::BucketId(1),
                             kTestStorageKey,
                             blink::mojom::StorageType::kTemporary,
                             /*is_default=*/false);

  // Default initializing kFrameId simulates a frame that is always active.
  const GlobalRenderFrameHostId kFrameId;
  const FileSystemAccessManagerImpl::BindingContext kBindingContext = {
      kTestStorageKey, kTestURL, kFrameId};

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FileSystemAccessLockManagerTest, ExclusiveLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();
  {
    auto exclusive_lock =
        TakeLockSync(kBindingContext, url, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SharedLock) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type_1 = manager_->CreateSharedLockTypeForTesting();
  LockType shared_lock_type_2 = manager_->CreateSharedLockTypeForTesting();
  {
    auto shared_lock = TakeLockSync(kBindingContext, url, shared_lock_type_1);
    ASSERT_TRUE(shared_lock);

    // Can take another shared lock of the same type, but not an exclusive lock
    // or a shared lock of another type.
    ASSERT_TRUE(TakeLockSync(kBindingContext, url, shared_lock_type_1));
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, shared_lock_type_2));
  }

  // The shared locks have been released and we should be available to acquire
  // an exclusive lock.
  ASSERT_TRUE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, SandboxedFile) {
  auto url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  url.SetBucket(kTestBucketLocator);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();
  {
    auto exclusive_lock =
        TakeLockSync(kBindingContext, url, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock while the file is exclusively locked.
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(kBindingContext, url, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(TakeLockSync(kBindingContext, url, exclusive_lock_type));
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
  auto exclusive_lock1 =
      TakeLockSync(kBindingContext, url1, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(TakeLockSync(kBindingContext, url1, exclusive_lock_type));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 =
      TakeLockSync(kBindingContext, url2, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(TakeLockSync(kBindingContext, url2, exclusive_lock_type));
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
  auto exclusive_lock1 =
      TakeLockSync(kBindingContext, url1, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock1);
  ASSERT_FALSE(TakeLockSync(kBindingContext, url1, exclusive_lock_type));

  // Can still take a lock on the file in the second file system.
  auto exclusive_lock2 =
      TakeLockSync(kBindingContext, url2, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock2);
  ASSERT_FALSE(TakeLockSync(kBindingContext, url2, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, DifferentBackends) {
  // We'll use the same path and pretend they're from different backends.
  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");

  // File on a local file system.
  auto local_url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  // File with the same path on an external file system.
  auto external_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(PathType::kExternal, path));

  EXPECT_EQ(local_url.path(), external_url.virtual_path());

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  // Take a lock on the file in the local file system.
  auto local_exclusive_lock =
      TakeLockSync(kBindingContext, local_url, exclusive_lock_type);
  ASSERT_TRUE(local_exclusive_lock);
  ASSERT_FALSE(TakeLockSync(kBindingContext, local_url, exclusive_lock_type));

  // Can still take a lock on the file in the external file system.
  auto external_exclusive_lock =
      TakeLockSync(kBindingContext, external_url, exclusive_lock_type);
  ASSERT_TRUE(external_exclusive_lock);
  ASSERT_FALSE(
      TakeLockSync(kBindingContext, external_url, exclusive_lock_type));
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
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  {
    auto exclusive_lock =
        TakeLockSync(kBindingContext, url1, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Other sites cannot access the file while it is exclusively locked.
    ASSERT_FALSE(TakeLockSync(kBindingContext, url2, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(kBindingContext, url2, shared_lock_type));
  }

  // The exclusive lock has been released and should be available to be
  // re-acquired.
  ASSERT_TRUE(TakeLockSync(kBindingContext, url2, exclusive_lock_type));
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocks) {
  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocksExternal) {
  base::FilePath parent_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(PathType::kExternal, parent_path));
  auto child_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(PathType::kExternal, parent_path.AppendASCII("child")));

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessLockManagerTest, AncestorLocksSandboxed) {
  auto parent_path = base::FilePath::FromUTF8Unsafe("test/foo/bar");
  auto parent_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, parent_path);
  parent_url.SetBucket(kTestBucketLocator);
  auto child_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      parent_path.AppendASCII("child"));
  child_url.SetBucket(kTestBucketLocator);

  AssertAncestorLockBehavior(parent_url, child_url);
}

TEST_F(FileSystemAccessLockManagerTest, AncestorWithSameName) {
  {
    base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
    auto parent_url =
        manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
    auto child_url = manager_->CreateFileSystemURLFromPath(
        PathInfo(parent_path.AppendASCII("foo")));

    AssertAncestorLockBehavior(parent_url, child_url);
  }

  {
    base::FilePath parent_path =
        base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");
    auto parent_url = manager_->CreateFileSystemURLFromPath(
        PathInfo(PathType::kExternal, parent_path));
    auto child_url = manager_->CreateFileSystemURLFromPath(
        PathInfo(PathType::kExternal, parent_path.AppendASCII("foo")));

    AssertAncestorLockBehavior(parent_url, child_url);
  }

  {
    auto parent_path = base::FilePath::FromUTF8Unsafe("test/foo/bar");
    auto parent_url = file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTemporary, parent_path);
    parent_url.SetBucket(kTestBucketLocator);
    auto child_url = file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTemporary,
        parent_path.AppendASCII("foo"));
    child_url.SetBucket(kTestBucketLocator);

    AssertAncestorLockBehavior(parent_url, child_url);
  }
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheExclusive) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_1;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_2;
  {
    auto exclusive_lock =
        TakeLockSync(bf_cache_context, url, exclusive_lock_type);
    ASSERT_TRUE(exclusive_lock);

    // Cannot take another lock of any type while the page is active.
    ASSERT_FALSE(TakeLockSync(active_context, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, url, shared_lock_type));

    // Entering into the BFCache should not evict the page.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // Taking a lock of a contentious type will not return synchronously, but
    // will start eviction and create a pending lock.
    manager_->TakeLock(active_context, url, shared_lock_type,
                       pending_future_1.GetCallback());
    ASSERT_FALSE(pending_future_1.IsReady());
    EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

    // Taking a lock that's not contentious with the pending lock will also
    // create a pending lock.
    manager_->TakeLock(active_context, url, shared_lock_type,
                       pending_future_2.GetCallback());
    ASSERT_FALSE(pending_future_2.IsReady());

    // Taking a lock that's contentious with the pending lock will fail if the
    // pending lock is still held by an active page.
    ASSERT_FALSE(TakeLockSync(active_context, url, exclusive_lock_type));
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future_1.IsReady());
  ASSERT_TRUE(pending_future_1.Take());
  ASSERT_TRUE(pending_future_2.IsReady());
  ASSERT_TRUE(pending_future_2.Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheShared) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type_1 = manager_->CreateSharedLockTypeForTesting();
  LockType shared_lock_type_2 = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_1;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_2;
  {
    auto shared_lock = TakeLockSync(bf_cache_context, url, shared_lock_type_1);
    ASSERT_TRUE(shared_lock);

    // Can only take shared locks of the same type.
    ASSERT_FALSE(TakeLockSync(active_context, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, url, shared_lock_type_2));
    ASSERT_TRUE(TakeLockSync(active_context, url, shared_lock_type_1));

    // Entering into the BFCache should not evict the page. The lock should not
    // have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    {
      // Taking a shared lock of the same type should succeed and not evict the
      // page.
      auto shared_lock_2 =
          TakeLockSync(active_context, url, shared_lock_type_1);
      ASSERT_TRUE(shared_lock_2);
      EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

      // While there's an active page holding the lock, taking a lock of a
      // contentious type will still fail.
      ASSERT_FALSE(TakeLockSync(active_context, url, exclusive_lock_type));
      ASSERT_FALSE(TakeLockSync(active_context, url, shared_lock_type_2));
    }

    // When only inactive pages hold the lock, taking a lock of a contentious
    // type will evict the page and create the lock asynchronously. The new lock
    // is pending in the lock manager until the evicting locks are destroyed.
    manager_->TakeLock(active_context, url, shared_lock_type_2,
                       pending_future_1.GetCallback());
    ASSERT_FALSE(pending_future_1.IsReady());

    // Taking a lock that's not contentious with the pending lock will also
    // create a pending lock.
    manager_->TakeLock(active_context, url, shared_lock_type_2,
                       pending_future_2.GetCallback());
    ASSERT_FALSE(pending_future_2.IsReady());

    // Taking a lock that's contentious with the pending lock will fail if the
    // pending lock is still held by an active page.
    ASSERT_FALSE(TakeLockSync(active_context, url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, url, shared_lock_type_1));
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future_1.IsReady());
  ASSERT_TRUE(pending_future_1.Take());
  ASSERT_TRUE(pending_future_2.IsReady());
  ASSERT_TRUE(pending_future_2.Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheTakeChildThenParent) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child")));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_1;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_2;
  {
    auto child_lock =
        TakeLockSync(bf_cache_context, child_url, shared_lock_type);
    ASSERT_TRUE(child_lock);

    // Entering into the BFCache should not evict the page. The lock should
    // not have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // When only inactive pages hold the child lock, taking a lock on an
    // ancestor will evict the lock and create the new lock asynchronously.
    // The new lock is pending in the lock manager until the evicting locks
    // are destroyed.
    manager_->TakeLock(active_context, parent_url, shared_lock_type,
                       pending_future_1.GetCallback());
    ASSERT_FALSE(pending_future_1.IsReady());

    // Taking a lock that's not contentious with the pending lock will also
    // create a pending lock.
    manager_->TakeLock(active_context, parent_url, shared_lock_type,
                       pending_future_2.GetCallback());
    ASSERT_FALSE(pending_future_2.IsReady());

    // Taking a lock that's contentious with the pending lock will fail if the
    // pending lock is still held by an active page.
    ASSERT_FALSE(TakeLockSync(active_context, parent_url, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, child_url, shared_lock_type));
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future_1.IsReady());
  ASSERT_TRUE(pending_future_1.Take());
  ASSERT_TRUE(pending_future_2.IsReady());
  ASSERT_TRUE(pending_future_2.Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheTakeParentThenChild) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url_1 = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child1")));
  auto child_url_2 = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child2")));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_1;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_2;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future_3;
  {
    auto parent_lock =
        TakeLockSync(bf_cache_context, parent_url, shared_lock_type);
    ASSERT_TRUE(parent_lock);

    // Entering into the BFCache should not evict the page. The lock should
    // not have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // When only inactive pages hold the parent lock, taking a lock on a
    // descendant will evict the lock and create the new lock asynchronously.
    // The new lock is pending in the lock manager until the evicting locks are
    // destroyed.
    manager_->TakeLock(active_context, child_url_1, shared_lock_type,
                       pending_future_1.GetCallback());
    ASSERT_FALSE(pending_future_1.IsReady());

    // Taking a lock that's not contentious with the pending lock will also
    // create a pending lock.
    manager_->TakeLock(active_context, child_url_1, shared_lock_type,
                       pending_future_2.GetCallback());
    ASSERT_FALSE(pending_future_2.IsReady());

    // Taking a lock where there isn't an existing lock but its a child of a
    // pending lock will create the lock asynchronously.
    manager_->TakeLock(active_context, child_url_2, exclusive_lock_type,
                       pending_future_3.GetCallback());
    ASSERT_FALSE(pending_future_1.IsReady());

    // Taking a lock that's contentious with a pending lock will fail if the
    // pending lock is still held by an active page.
    ASSERT_FALSE(
        TakeLockSync(active_context, child_url_1, exclusive_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, child_url_2, shared_lock_type));
    ASSERT_FALSE(TakeLockSync(active_context, parent_url, shared_lock_type));
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future_1.IsReady());
  ASSERT_TRUE(pending_future_1.Take());
  ASSERT_TRUE(pending_future_2.IsReady());
  ASSERT_TRUE(pending_future_2.Take());
  ASSERT_TRUE(pending_future_3.IsReady());
  ASSERT_TRUE(pending_future_3.Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictPendingLockRoot) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_and_evicting_future;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future;
  {
    auto shared_lock = TakeLockSync(bf_cache_context, url, exclusive_lock_type);
    ASSERT_TRUE(shared_lock);

    // Entering into the BFCache should not evict the page. The lock should not
    // have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // Reuse the bf_cache_context as if it were another page.
    //
    // This works because `FileSystemAccessManagerImpl` doesn't check if the
    // context is inactive when `TakeLock` is called. But now any `Lock` taken
    // with `bf_cache_context_2` will be held only by an inactive pages.
    auto& bf_cache_context_2 = bf_cache_context;

    // When only inactive pages hold the lock, taking a lock of a contentious
    // type will evict the page and create the lock asynchronously. The new lock
    // is pending in the lock manager until the evicting locks are destroyed.
    manager_->TakeLock(bf_cache_context_2, url, exclusive_lock_type,
                       pending_and_evicting_future.GetCallback());
    ASSERT_FALSE(pending_and_evicting_future.IsReady());
    EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

    // If only inactive pages hold the pending lock, then taking a lock of a
    // contentious type will also evict the pending lock and create the new lock
    // asynchronously.
    manager_->TakeLock(active_context, url, shared_lock_type,
                       pending_future.GetCallback());
    ASSERT_FALSE(pending_future.IsReady());
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future.IsReady());
  ASSERT_TRUE(pending_future.Take());

  // The pending lock that got evicted will not have its callback run.
  ASSERT_FALSE(pending_and_evicting_future.IsReady());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictDescendantPendingLockRoot) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child")));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_and_evicting_future;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future;
  {
    auto child_lock =
        TakeLockSync(bf_cache_context, child_url, exclusive_lock_type);
    ASSERT_TRUE(child_lock);

    // Entering into the BFCache should not evict the page. The lock should not
    // have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // Reuse the bf_cache_context as if it were another page.
    //
    // This works because `FileSystemAccessManagerImpl` doesn't check if the
    // context is inactive when `TakeLock` is called. But now any `Lock` taken
    // with `bf_cache_context_2` will be held only by an inactive pages.
    auto& bf_cache_context_2 = bf_cache_context;

    // When only inactive pages hold the child lock, taking a contentious lock
    // on the child will evict the page and create the new lock asynchronously.
    // The new child lock is pending in the lock manager until the old child
    // lock is evicted.
    manager_->TakeLock(bf_cache_context_2, child_url, exclusive_lock_type,
                       pending_and_evicting_future.GetCallback());
    ASSERT_FALSE(pending_and_evicting_future.IsReady());
    EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

    // If only inactive pages hold the pending child lock, then taking a lock on
    // an ancestor will also evict the pending lock and create the ancestor lock
    // asynchronously.
    manager_->TakeLock(bf_cache_context_2, parent_url, exclusive_lock_type,
                       pending_future.GetCallback());
    ASSERT_FALSE(pending_future.IsReady());
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future.IsReady());
  ASSERT_TRUE(pending_future.Take());

  // The pending lock that got evicted will not have its callback run.
  ASSERT_FALSE(pending_and_evicting_future.IsReady());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictAncestorPendingLockRoot) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child")));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_and_evicting_future;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future;
  {
    auto child_lock =
        TakeLockSync(bf_cache_context, child_url, exclusive_lock_type);
    ASSERT_TRUE(child_lock);

    // Entering into the BFCache should not evict the page. The lock should not
    // have been released.
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

    // Reuse the bf_cache_context as if it were another page.
    //
    // This works because `FileSystemAccessManagerImpl` doesn't check if the
    // context is inactive when `TakeLock` is called. But now any `Lock` taken
    // with `bf_cache_context_2` will be held only by an inactive pages.
    auto& bf_cache_context_2 = bf_cache_context;

    // When only inactive pages hold the child lock, taking a lock on an
    // ancestor will evict the page and create the lock asynchronously. The
    // ancestor lock is pending in the lock manager until the child lock is
    // evicted.
    manager_->TakeLock(bf_cache_context_2, parent_url, shared_lock_type,
                       pending_and_evicting_future.GetCallback());
    ASSERT_FALSE(pending_and_evicting_future.IsReady());
    EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

    // If only inactive pages hold the pending ancestor lock, then taking a lock
    // on a descendant of it will evict the pending ancestor lock and create the
    // descendant lock asynchronously.
    manager_->TakeLock(active_context, child_url, exclusive_lock_type,
                       pending_future.GetCallback());
    ASSERT_FALSE(pending_future.IsReady());

    // Taking a lock that's contentious with a pending lock will fail if the
    // pending lock is still held by an active page.
    ASSERT_FALSE(TakeLockSync(active_context, parent_url, shared_lock_type));
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future.IsReady());
  ASSERT_TRUE(pending_future.Take());

  // The pending locks that got evicted will not have its callback run.
  ASSERT_FALSE(pending_and_evicting_future.IsReady());
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCacheEvictMultipleDescendantPendingLockRoot) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url =
      manager_->CreateFileSystemURLFromPath(PathInfo(parent_path));
  auto child_url_1 = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child1")));
  auto child_url_2 = manager_->CreateFileSystemURLFromPath(
      PathInfo(parent_path.AppendASCII("child2")));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_and_evicting_future_1;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_and_evicting_future_2;
  base::test::TestFuture<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
      pending_future;
  {
    auto child_lock_1 =
        TakeLockSync(bf_cache_context, child_url_1, exclusive_lock_type);
    ASSERT_TRUE(child_lock_1);

    {
      auto child_lock_2 =
          TakeLockSync(bf_cache_context, child_url_2, exclusive_lock_type);
      ASSERT_TRUE(child_lock_2);

      // Entering into the BFCache should not evict the page. The lock should
      // not have been released.
      rfh->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
      EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

      // Reuse the bf_cache_context as if it were another page.
      //
      // This works because `FileSystemAccessManagerImpl` doesn't check if the
      // context is inactive when `TakeLock` is called. But now any `Lock` taken
      // with `bf_cache_context_2` will be held only by an inactive pages.
      auto& bf_cache_context_2 = bf_cache_context;

      // When only inactive pages hold the child locks, taking a contentious
      // locks on them will evict the page and create the new locks
      // asynchronously. The new child locks are pending in the lock manager
      // until the old child locks are evicted.
      manager_->TakeLock(bf_cache_context_2, child_url_1, exclusive_lock_type,
                         pending_and_evicting_future_1.GetCallback());
      ASSERT_FALSE(pending_and_evicting_future_1.IsReady());
      manager_->TakeLock(bf_cache_context_2, child_url_2, exclusive_lock_type,
                         pending_and_evicting_future_2.GetCallback());
      ASSERT_FALSE(pending_and_evicting_future_2.IsReady());
      EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

      // If only inactive pages hold the pending child locks, then taking a lock
      // on an ancestor will also evict the pending locks and create the
      // ancestor lock asynchronously.
      manager_->TakeLock(bf_cache_context_2, parent_url, exclusive_lock_type,
                         pending_future.GetCallback());
      ASSERT_FALSE(pending_future.IsReady());
    }
    // Both child locks must be evicted before the parent lock can be created.
    ASSERT_FALSE(pending_future.IsReady());

    // The pending lock that got evicted will not have its callback run.
    ASSERT_FALSE(pending_and_evicting_future_2.IsReady());
  }
  // Once the lock we're evicting has been destroyed, the callbacks for the
  // pending locks will run with a handle for the new lock.
  ASSERT_TRUE(pending_future.IsReady());
  ASSERT_TRUE(pending_future.Take());

  // The pending locks that got evicted will not have their callbacks run.
  ASSERT_FALSE(pending_and_evicting_future_1.IsReady());
  ASSERT_FALSE(pending_and_evicting_future_2.IsReady());
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCachePendingLockDestroyedOnPromotion) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  auto exclusive_lock =
      TakeLockSync(bf_cache_context, url, exclusive_lock_type);
  ASSERT_TRUE(exclusive_lock);

  // Entering into the BFCache should not evict the page.
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  EXPECT_FALSE(rfh->is_evicted_from_back_forward_cache());

  // Taking a lock of a contentious type will not return synchronously, but
  // will start eviction and create a pending lock.
  bool pending_lock_callback_run = false;
  auto pending_callback = base::BindOnce(
      [](bool* pending_lock_callback_run,
         scoped_refptr<FileSystemAccessLockManager::LockHandle> lock_handle) {
        // Resetting the `lock_handle` will destroy the promoted Pending
        // Lock since its the only `LockHandle` to it.
        lock_handle.reset();
        *pending_lock_callback_run = true;
      },
      &pending_lock_callback_run);
  manager_->TakeLock(active_context, url, shared_lock_type,
                     std::move(pending_callback));
  EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());

  // Resetting the `exclusive_lock` destroys the exclusive lock since its the
  // only LockHandle to it. This promotes the Pending Lock to Taken but it is
  // destroyed before its pending callbacks return.
  EXPECT_FALSE(pending_lock_callback_run);
  exclusive_lock.reset();
  EXPECT_TRUE(pending_lock_callback_run);
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCacheDescendantPendingLockDestroyedOnPromotion) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(parent.AppendASCII("child"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` to create a Pending child lock. Destroy its future
  // so its handle is destroyed on promotion.
  auto child_future =
      TakeLockAsync(active_context, child_url, exclusive_lock_type);
  child_future.reset();

  // Finish evicting the original `parent_lock` causing child_future to be
  // promoted but immediately destroyed on promotion.
  parent_lock.reset();
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCacheMultipleDescendantPendingLockDestroyedOnPromotion) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  auto parent_url = CreateLocalUrl(parent);
  auto child1_url = CreateLocalUrl(parent.AppendASCII("child1"));
  auto child2_url = CreateLocalUrl(parent.AppendASCII("child2"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` to create a Pending child1 lock. Destroy its future
  // so its handle is destroyed on promotion.
  auto child1_future =
      TakeLockAsync(active_context, child1_url, exclusive_lock_type);
  child1_future.reset();

  // Create a child2 lock which is Pending since the ancestor Lock is Pending on
  // `parent_lock` being evicted. Destroy its future so its handle is destroyed
  // on promotion.
  auto child2_future =
      TakeLockAsync(active_context, child2_url, exclusive_lock_type);
  child2_future.reset();

  // Finish evicting the original `parent_lock` causing child_future1 and
  // child_future2 to be promoted but immediately destroyed on promotion.
  parent_lock.reset();
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictPendingChild) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(parent.AppendASCII("ipsum1074"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` to create a Pending child lock.
  auto child_future1 =
      TakeLockAsync(bf_cache_context, child_url, exclusive_lock_type);

  // Evict the Pending child lock to create another Pending child lock.
  auto child_future2 =
      TakeLockAsync(active_context, child_url, exclusive_lock_type);

  // Finish evicting the original `parent_lock` causing child_future2 to be
  // promoted, but not child_future1.
  parent_lock.reset();
  ASSERT_FALSE(child_future1->IsReady());
  ASSERT_TRUE(child_future2->IsReady() && child_future2->Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictAncestorOfPendingTree) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  base::FilePath child = parent.AppendASCII("child");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(child);
  auto grandchild_url = CreateLocalUrl(child.AppendASCII("grandchild"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType shared_lock_type = manager_->CreateSharedLockTypeForTesting();

  auto child_lock = TakeLockSync(bf_cache_context, child_url, shared_lock_type);
  ASSERT_TRUE(child_lock);

  // Evict the `child_lock` to create a Pending grandchild lock.
  auto grandchild_future =
      TakeLockAsync(bf_cache_context, grandchild_url, exclusive_lock_type);

  // Evict the Pending grandchild lock to create a Pending parent lock.
  auto parent_future =
      TakeLockAsync(bf_cache_context, parent_url, exclusive_lock_type);

  // Finish evicting the original `child_lock` causing parent_future to be
  // promoted, but not grandchild_future.
  child_lock.reset();
  ASSERT_FALSE(grandchild_future->IsReady());
  ASSERT_TRUE(parent_future->IsReady() && parent_future->Take());
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCacheEvictPendingAncestorOfEvictedLock) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  base::FilePath child = parent.AppendASCII("child");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(child);
  auto grandchild_url = CreateLocalUrl(child.AppendASCII("grandchild"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` to create a Pending grandchild lock.
  auto grandchild_future1 =
      TakeLockAsync(bf_cache_context, grandchild_url, exclusive_lock_type);

  // Evict the Pending grandchild lock to create another Pending grandchild
  // lock.
  auto grandchild_future2 =
      TakeLockAsync(bf_cache_context, grandchild_url, exclusive_lock_type);

  // Evict the Pending grandchild lock to create a Pending child lock.
  auto child_future =
      TakeLockAsync(bf_cache_context, child_url, exclusive_lock_type);

  // Finish evicting the original `parent_lock` causing child_future to be
  // promoted, but not grandchild_future1 or grandchild_future2.
  parent_lock.reset();
  ASSERT_FALSE(grandchild_future1->IsReady());
  ASSERT_FALSE(grandchild_future2->IsReady());
  ASSERT_TRUE(child_future->IsReady() && child_future->Take());
}

TEST_F(FileSystemAccessLockManagerTest,
       BFCacheEvictPendingDescendantOfEvictedLock) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  base::FilePath child = parent.AppendASCII("child");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(child);
  auto grandchild_url = CreateLocalUrl(child.AppendASCII("grandchild"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` to create a Pending grandchild lock.
  auto grandchild_future1 =
      TakeLockAsync(bf_cache_context, grandchild_url, exclusive_lock_type);

  // Evict the Pending grandchild lock to create a Pending child lock.
  auto child_future =
      TakeLockAsync(bf_cache_context, child_url, exclusive_lock_type);

  // Evict the Pending child lock to create a Pending grandchild lock.
  auto grandchild_future2 =
      TakeLockAsync(bf_cache_context, grandchild_url, exclusive_lock_type);

  // Finish evicting the original `parent_lock` causing grandchild_future2 to be
  // promoted, but not grandchild_future1 or child_future.
  parent_lock.reset();
  ASSERT_FALSE(grandchild_future1->IsReady());
  ASSERT_FALSE(child_future->IsReady());
  ASSERT_TRUE(grandchild_future2->IsReady() && grandchild_future2->Take());
}

TEST_F(FileSystemAccessLockManagerTest, BFCacheEvictPendingTree) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  auto active_context = kBindingContext;
  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetGlobalId());
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  base::FilePath parent = dir_.GetPath().AppendASCII("parent");
  auto parent_url = CreateLocalUrl(parent);
  auto child_url = CreateLocalUrl(parent.AppendASCII("child"));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();

  auto parent_lock =
      TakeLockSync(bf_cache_context, parent_url, exclusive_lock_type);
  ASSERT_TRUE(parent_lock);

  // Evict the `parent_lock` by taking a lock on its child.
  auto child_future1 =
      TakeLockAsync(bf_cache_context, child_url, exclusive_lock_type);

  // Evict the Pending child lock to create a new Pending child lock.
  auto child_future2 =
      TakeLockAsync(bf_cache_context, child_url, exclusive_lock_type);

  // Evict the Pending ancestor parent lock of of the Pending child to create a
  // new Pending exclusive lock on the parent.
  auto parent_future =
      TakeLockAsync(bf_cache_context, parent_url, exclusive_lock_type);

  // Finish evicting the original `parent_lock` causing parent_future to be
  // promoted, but not child_future1 or child_future2.
  parent_lock.reset();
  ASSERT_FALSE(child_future1->IsReady());
  ASSERT_FALSE(child_future2->IsReady());
  ASSERT_TRUE(parent_future->IsReady() && parent_future->Take());
}

TEST_F(FileSystemAccessLockManagerTest,
       LocksCanExistAfterFileSystemAccessManagerIsDestroyed) {
  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  auto exclusive_lock = TakeLockSync(kBindingContext, url, exclusive_lock_type);

  base::WeakPtr<FileSystemAccessLockManager> lock_manager_weak_ptr =
      manager_->GetLockManagerWeakPtrForTesting();

  // Destroy the `FileSystemAccessManager` which holds a `scoped_refptr` to the
  // `FileSystemAccessLockManager`.
  manager_.reset();

  // The `FileSystemAccessLockManager` stays alive despite the
  // `FileSystemAccessManager` being destroyed.
  ASSERT_TRUE(lock_manager_weak_ptr);

  // The `exclusive_lock` is the only thing keeping the
  // `FileSystemAccessLockManager` alive. So destroying it should destroy the
  // `FileSystemAccessLockManager`.
  exclusive_lock.reset();
  ASSERT_FALSE(lock_manager_weak_ptr);
}

}  // namespace content
