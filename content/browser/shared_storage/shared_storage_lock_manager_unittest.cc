// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_lock_manager.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using AccessScope = blink::SharedStorageAccessScope;

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
    web_locks_feature_.InitAndEnableFeature(
        blink::features::kSharedStorageWebLocks);
    transactional_batch_update_feature_.InitAndEnableFeature(
        network::features::kSharedStorageTransactionalBatchUpdate);
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

  void CreateExternalLockRequest1(
      const url::Origin& origin,
      const std::string& lock_name,
      blink::mojom::LockMode lock_mode,
      blink::mojom::LockManager::WaitMode wait_mode) {
    CreateExternalLockRequestHelper(origin, lock_name, lock_mode, wait_mode,
                                    /*request_number=*/1);
  }

  void CreateExternalLockRequest2(
      const url::Origin& origin,
      const std::string& lock_name,
      blink::mojom::LockMode lock_mode,
      blink::mojom::LockManager::WaitMode wait_mode) {
    CreateExternalLockRequestHelper(origin, lock_name, lock_mode, wait_mode,
                                    /*request_number=*/2);
  }

  std::u16string SharedStorageGet(const url::Origin& origin,
                                  const std::u16string& key) {
    base::test::TestFuture<storage::SharedStorageManager::GetResult> future;
    StoragePartition().GetSharedStorageManager()->Get(origin, key,
                                                      future.GetCallback());
    return future.Take().data;
  }

 protected:
  std::unique_ptr<SharedStorageLockManager> test_lock_manager_;
  std::unique_ptr<TestLockRequest> external_lock_request1_;
  std::unique_ptr<TestLockRequest> external_lock_request2_;
  base::test::ScopedFeatureList web_locks_feature_;
  base::test::ScopedFeatureList transactional_batch_update_feature_;

 private:
  void CreateExternalLockRequestHelper(
      const url::Origin& origin,
      const std::string& lock_name,
      blink::mojom::LockMode lock_mode,
      blink::mojom::LockManager::WaitMode wait_mode,
      size_t request_number) {
    DCHECK(request_number == 1 || request_number == 2);

    mojo::Remote<blink::mojom::LockManager>& external_lock_manager =
        (request_number == 1) ? external_lock_manager1_
                              : external_lock_manager2_;
    std::unique_ptr<TestLockRequest>& external_lock_request =
        (request_number == 1) ? external_lock_request1_
                              : external_lock_request2_;

    DCHECK(!external_lock_manager);
    DCHECK(!external_lock_request);

    mojo::PendingRemote<blink::mojom::LockManager> pending_lock_manager;
    test_lock_manager_->BindLockManager(
        origin, pending_lock_manager.InitWithNewPipeAndPassReceiver());
    external_lock_manager.Bind(std::move(pending_lock_manager));

    mojo::PendingAssociatedReceiver<blink::mojom::LockRequest>
        lock_request_receiver;

    mojo::PendingAssociatedRemote<blink::mojom::LockRequest>
        lock_request_remote =
            lock_request_receiver.InitWithNewEndpointAndPassRemote();

    external_lock_request =
        std::make_unique<TestLockRequest>(std::move(lock_request_receiver));

    external_lock_manager->RequestLock(lock_name, lock_mode, wait_mode,
                                       std::move(lock_request_remote));
  }

  mojo::Remote<blink::mojom::LockManager> external_lock_manager1_;
  mojo::Remote<blink::mojom::LockManager> external_lock_manager2_;
};

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Create(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // There's no other outstanding lock request. Thus, the `SharedStorageUpdate`
  // is immediately handled.
  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_WaitForGranted) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Create(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The external lock is granted but not yet released. The
  // `SharedStorageUpdate()` request is still waiting.
  EXPECT_TRUE(external_lock_request1_->granted());
  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");

  // After the external lock is released, the `SharedStorageUpdate()` gets
  // handled.
  external_lock_request1_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithoutLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/std::nullopt);

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Create(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The external lock is granted but not yet released. The
  // `SharedStorageUpdate()` request has been handled, as it does not request
  // the lock.
  EXPECT_TRUE(external_lock_request1_->granted());
  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
}

TEST_F(SharedStorageLockManagerTest,
       SharedStorageUpdateWithLock_BlockingExternalLockRequest) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  auto method_with_options =
      MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                     /*ignore_if_present=*/true, /*with_lock=*/"lock1");

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageUpdate(
      std::move(method_with_options), origin,
      AccessScope::kSharedStorageWorklet,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Create(),
      error_message_future.GetCallback());

  EXPECT_FALSE(error_message_future.IsReady());

  // Create an external lock request with WaitMode::NO_WAIT when the lock for
  // `SharedStorageUpdate` is not yet released. The second request is expected
  // to fail.
  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::NO_WAIT);

  task_environment()->RunUntilIdle();

  EXPECT_TRUE(external_lock_request1_->failed());
  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
}

TEST_F(SharedStorageLockManagerTest, BatchUpdateWithLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"c", /*value=*/u"d",
                                                /*ignore_if_present=*/true));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/"lock1", origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // There's no other outstanding lock request. Thus, the
  // `SharedStorageBatchUpdate` is immediately handled.
  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");
}

TEST_F(SharedStorageLockManagerTest, BatchUpdateWithLock_WaitForGranted) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"c", /*value=*/u"d",
                                                /*ignore_if_present=*/true));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/"lock1", origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The external lock is granted but not yet released. The
  // `SharedStorageBatchUpdate()` request is still waiting.
  EXPECT_TRUE(external_lock_request1_->granted());
  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"");

  // After the external lock is released, the `SharedStorageBatchUpdate()` gets
  // handled.
  external_lock_request1_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");
}

class SharedStorageLockManagerTransactionalBatchUpdateDisabledTest
    : public SharedStorageLockManagerTest {
 public:
  SharedStorageLockManagerTransactionalBatchUpdateDisabledTest() {
    transactional_batch_update_feature_.Reset();
    transactional_batch_update_feature_.InitAndDisableFeature(
        network::features::kSharedStorageTransactionalBatchUpdate);
  }
};

// Test `SharedStorageBatchUpdate` with two methods. The first method requests a
// lock and waits for it to be granted. The second method is handled first.
TEST_F(SharedStorageLockManagerTransactionalBatchUpdateDisabledTest,
       BatchUpdate_FirstMethodLockWaitForGranted) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock1"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"c", /*value=*/u"d",
                                                /*ignore_if_present=*/true));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/std::nullopt, origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The external lock is granted but not yet released. The batch update hasn't
  // completely finished, but the second method within the batch has finished.
  EXPECT_TRUE(external_lock_request1_->granted());
  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");

  // After the external lock is released, the first method within the batch
  // gets handled.
  external_lock_request1_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");
}

// Test `SharedStorageBatchUpdate` with two methods, both requesting locks and
// waiting for them to be granted. The second method's lock is granted first,
// and thus the second method is handled first.
TEST_F(SharedStorageLockManagerTransactionalBatchUpdateDisabledTest,
       BatchUpdate_SecondMethodLockGrantedFirst) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  CreateExternalLockRequest1(origin, /*lock_name=*/"lock1",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  CreateExternalLockRequest2(origin, /*lock_name=*/"lock2",
                             blink::mojom::LockMode::EXCLUSIVE,
                             blink::mojom::LockManager::WaitMode::WAIT);

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock1"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"c", /*value=*/u"d",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock2"));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/std::nullopt, origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The external locks are granted but not yet released. None of the methods
  // within the batch has finished.
  EXPECT_TRUE(external_lock_request1_->granted());
  EXPECT_TRUE(external_lock_request2_->granted());
  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"");

  // After the external 'lock2' is released, the batch update hasn't completely
  // finished, but the second method within the batch has finished.
  external_lock_request2_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");

  // After the external 'lock1' is released, the first method within the batch
  // gets handled.
  external_lock_request1_->ReleaseLock();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_TRUE(error_message_future.Take().empty());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");
}

TEST_F(SharedStorageLockManagerTransactionalBatchUpdateDisabledTest,
       BatchUpdate_BatchLockSameWithInnerMethodLock_Deadlock) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock1"));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/"lock1", origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // The batch lock is blocking the inner method lock, but the batch lock won't
  // be released until the inner method gets handled. A deadlock has occurred.
  // Note that this is a user-level error.
  EXPECT_FALSE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"");
}

TEST_F(SharedStorageLockManagerTransactionalBatchUpdateDisabledTest,
       BatchUpdate_TwoInnerMethodsWithSameLock_ImmediatelyHandled) {
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock1"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"c", /*value=*/u"d",
                                                /*ignore_if_present=*/true,
                                                /*with_lock=*/"lock1"));

  base::test::TestFuture<const std::string&> error_message_future;

  test_lock_manager_->SharedStorageBatchUpdate(
      std::move(methods_with_options),
      /*with_lock=*/std::nullopt, origin, AccessScope::kWindow,
      /*main_frame_id=*/GlobalRenderFrameHostId(),
      /*worklet_devtools_token=*/base::UnguessableToken::Null(),
      error_message_future.GetCallback());
  task_environment()->RunUntilIdle();

  // Each method acquires and releases the lock internally. The batch update
  // finishes immediately.
  EXPECT_TRUE(error_message_future.IsReady());
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"a"), u"b");
  EXPECT_EQ(SharedStorageGet(origin, /*key=*/u"c"), u"d");
}

}  // namespace content
