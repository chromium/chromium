// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_lock_manager.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using AccessScope = SharedStorageLockManager::AccessScope;

class TestLockRequest : public blink::mojom::LockRequest {
 public:
  explicit TestLockRequest(
      mojo::PendingAssociatedReceiver<blink::mojom::LockRequest> receiver)
      : receiver_(this, std::move(receiver)) {}

  // blink::mojom::LockRequest
  void Granted(mojo::PendingAssociatedRemote<blink::mojom::LockHandle>
                   pending_handle) override {
    granted_ = true;
    handle_.Bind(std::move(pending_handle));
  }

  void Failed() override { failed_ = true; }

  void ReleaseLock() {
    CHECK(granted_);
    handle_.reset();
  }

  bool granted() const { return granted_; }

  bool failed() const { return failed_; }

 private:
  bool granted_ = false;
  bool failed_ = false;

  mojo::AssociatedRemote<blink::mojom::LockHandle> handle_;

  mojo::AssociatedReceiver<blink::mojom::LockRequest> receiver_{this};
};

}  // namespace

// Test `SharedStorageLockManager`. Since `BindLockManager()` delegates to
// `LockManager::BindReceiver()`, we avoid retesting this function alone.
// Rather, we focus on testing other functions and their interactions with
// `BindLockManager()`.
class SharedStorageLockManagerTest : public RenderViewHostTestHarness {
 public:
  SharedStorageLockManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kSharedStorageWebLocks);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_lock_manager_ =
        std::make_unique<SharedStorageLockManager>(StoragePartition());
  }

  void TearDown() override {
    test_lock_manager_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  StoragePartitionImpl& StoragePartition() {
    return *static_cast<StoragePartitionImpl*>(
        web_contents()->GetBrowserContext()->GetDefaultStoragePartition());
  }

  void CreateExternalLockRequest(
      const url::Origin& origin,
      const std::string& lock_name,
      blink::mojom::LockMode lock_mode,
      blink::mojom::LockManager::WaitMode wait_mode) {
    DCHECK(!external_lock_manager_);
    DCHECK(!external_lock_request_);

    mojo::PendingRemote<blink::mojom::LockManager> pending_lock_manager;
    test_lock_manager_->BindLockManager(
        origin, pending_lock_manager.InitWithNewPipeAndPassReceiver());
    external_lock_manager_.Bind(std::move(pending_lock_manager));

    mojo::PendingAssociatedReceiver<blink::mojom::LockRequest>
        lock_request_receiver;

    mojo::PendingAssociatedRemote<blink::mojom::LockRequest>
        lock_request_remote =
            lock_request_receiver.InitWithNewEndpointAndPassRemote();

    external_lock_request_ =
        std::make_unique<TestLockRequest>(std::move(lock_request_receiver));

    external_lock_manager_->RequestLock(lock_name, lock_mode, wait_mode,
                                        std::move(lock_request_remote));
  }

 protected:
  std::unique_ptr<SharedStorageLockManager> test_lock_manager_;

  mojo::Remote<blink::mojom::LockManager> external_lock_manager_;
  std::unique_ptr<TestLockRequest> external_lock_request_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  std::optional<std::string> callback_error_message;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        callback_error_message = error_message;
      });

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/FrameTreeNodeId(), std::move(callback));
  task_environment()->RunUntilIdle();

  // There's no other outstanding lock request. Thus, the `SharedStorageUpdate`
  // is immediately handled.
  EXPECT_TRUE(callback_error_message);
  EXPECT_TRUE(callback_error_message->empty());
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_WaitForGranted) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest(origin, /*lock_name=*/"lock1",
                            blink::mojom::LockMode::EXCLUSIVE,
                            blink::mojom::LockManager::WaitMode::WAIT);

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  std::optional<std::string> callback_error_message;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        callback_error_message = error_message;
      });

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/FrameTreeNodeId(), std::move(callback));
  task_environment()->RunUntilIdle();

  // The first lock is granted but not yet released. The `SharedStorageUpdate()`
  // request is still waiting.
  EXPECT_TRUE(external_lock_request_->granted());
  EXPECT_FALSE(callback_error_message);

  // After the first lock is released, the `SharedStorageUpdate()` gets handled.
  external_lock_request_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(callback_error_message);
  EXPECT_TRUE(callback_error_message->empty());
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithoutLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest(origin, /*lock_name=*/"lock1",
                            blink::mojom::LockMode::EXCLUSIVE,
                            blink::mojom::LockManager::WaitMode::WAIT);

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/std::nullopt);

  std::optional<std::string> callback_error_message;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        callback_error_message = error_message;
      });

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/FrameTreeNodeId(), std::move(callback));
  task_environment()->RunUntilIdle();

  // The first lock is granted but not yet released. The `SharedStorageUpdate()`
  // request has been handled, as it does not request the lock.
  EXPECT_TRUE(external_lock_request_->granted());
  EXPECT_TRUE(callback_error_message);
  EXPECT_TRUE(callback_error_message->empty());
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_BlockingExternalLockRequest) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  std::optional<std::string> callback_error_message;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        callback_error_message = error_message;
      });

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/FrameTreeNodeId(), std::move(callback));

  EXPECT_FALSE(callback_error_message);

  // Create an external lock request with WaitMode::NO_WAIT when the lock for
  // `SharedStorageUpdate` is not yet released. The second request is expected
  // to fail.
  CreateExternalLockRequest(origin, /*lock_name=*/"lock1",
                            blink::mojom::LockMode::EXCLUSIVE,
                            blink::mojom::LockManager::WaitMode::NO_WAIT);

  task_environment()->RunUntilIdle();

  EXPECT_TRUE(external_lock_request_->failed());
  EXPECT_TRUE(callback_error_message);
  EXPECT_TRUE(callback_error_message->empty());
}

}  // namespace content
