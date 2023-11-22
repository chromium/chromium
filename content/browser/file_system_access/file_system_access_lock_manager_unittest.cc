// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
  FileSystemAccessLockManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessBFCache,
         blink::features::kFileSystemAccessLockingScheme},
        {});
  }

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

  scoped_refptr<FileSystemAccessLockManager::LockHandle> TakeLockSync(
      const FileSystemAccessManagerImpl::BindingContext binding_context,
      const storage::FileSystemURL& url,
      FileSystemAccessLockManager::LockType lock_type) {
    base::test::TestFuture<
        scoped_refptr<FileSystemAccessLockManager::LockHandle>>
        future;
    manager_->TakeLock(binding_context, url, lock_type, future.GetCallback());
    return future.Take();
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
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

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
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

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
  auto local_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

  // File with the same path on an external file system.
  auto external_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kExternal, path);

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

TEST_F(FileSystemAccessLockManagerTest, BFCacheExclusive) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);

  auto bf_cache_context = FileSystemAccessManagerImpl::BindingContext(
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url_1 = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child1")));
  auto child_url_2 = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child2")));

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath path = dir_.GetPath().AppendASCII("foo");
  auto url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, path);

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child")));

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
      kTestStorageKey, kTestURL, rfh->GetAssociatedRenderFrameHostId());
  auto active_context = kBindingContext;

  base::FilePath parent_path = dir_.GetPath().AppendASCII("foo");
  auto parent_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, parent_path);
  auto child_url_1 = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child1")));
  auto child_url_2 = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal,
      parent_path.Append(FILE_PATH_LITERAL("child2")));

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

}  // namespace content
