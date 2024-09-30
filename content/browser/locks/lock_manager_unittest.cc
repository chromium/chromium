// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/locks/lock_manager.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class LockManagerInvalidBucketTest : public testing::Test {
 public:
  void SetUp() override {
    pending_receiver_ = pending_remote_.InitWithNewPipeAndPassReceiver();

    storage::BucketId bucket_id = storage::BucketId();  // Invalid BucketId.

    lock_manager_.BindReceiver(bucket_id, std::move(pending_receiver_));

    remote_.Bind(std::move(pending_remote_));
  }

  mojo::Remote<blink::mojom::LockManager>& GetRemote() { return remote_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  LockManager<storage::BucketId> lock_manager_;
  mojo::PendingRemote<blink::mojom::LockManager> pending_remote_;
  mojo::PendingReceiver<blink::mojom::LockManager> pending_receiver_;
  mojo::Remote<blink::mojom::LockManager> remote_;
};

class TestLockRequest : public blink::mojom::LockRequest {
 public:
  explicit TestLockRequest(
      mojo::PendingAssociatedReceiver<blink::mojom::LockRequest>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  void Granted(mojo::PendingAssociatedRemote<blink::mojom::LockHandle>
                   lock_handle) override {
    remote_ = &lock_handle;
    granted_ = true;
    run_loop_.Quit();
  }

  void Failed() override {
    failed_ = true;
    run_loop_.Quit();
  }

  void WaitForCallback() { run_loop_.Run(); }

  bool FailureCalled() const { return failed_; }
  bool GrantedCalled() const { return granted_; }

 private:
  raw_ptr<mojo::PendingAssociatedRemote<blink::mojom::LockHandle>> remote_;
  mojo::AssociatedReceiver<blink::mojom::LockRequest> receiver_;
  base::RunLoop run_loop_;
  bool failed_ = false;
  bool granted_ = false;
};

TEST_F(LockManagerInvalidBucketTest, RequestLock) {
  mojo::PendingAssociatedRemote<blink::mojom::LockRequest> pending_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::LockRequest> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();
  TestLockRequest request(std::move(pending_receiver));

  GetRemote()->RequestLock("lock", blink::mojom::LockMode::EXCLUSIVE,
                           blink::mojom::LockManager::WaitMode::WAIT,
                           std::move(pending_remote));

  request.WaitForCallback();
  EXPECT_TRUE(request.FailureCalled());
  EXPECT_FALSE(request.GrantedCalled());
}

TEST_F(LockManagerInvalidBucketTest, QueryState) {
  base::RunLoop run_loop;

  GetRemote()->QueryState(
      base::BindOnce([](std::vector<blink::mojom::LockInfoPtr> first,
                        std::vector<blink::mojom::LockInfoPtr> second) {
        EXPECT_TRUE(first.empty());
        EXPECT_TRUE(second.empty());
      }).Then(run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace content
