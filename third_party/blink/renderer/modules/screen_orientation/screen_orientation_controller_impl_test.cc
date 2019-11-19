// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller_impl.h"

#include <memory>

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

using LockOrientationCallback =
    device::mojom::blink::ScreenOrientation::LockOrientationCallback;
using LockResult = device::mojom::blink::ScreenOrientationLockResult;

// MockLockOrientationCallback is an implementation of
// WebLockOrientationCallback and takes a LockOrientationResultHolder* as a
// parameter when being constructed. The |results_| pointer is owned by the
// caller and not by the callback object. The intent being that as soon as the
// callback is resolved, it will be killed so we use the
// LockOrientationResultHolder to know in which state the callback object is at
// any time.
class MockLockOrientationCallback : public blink::WebLockOrientationCallback {
 public:
  struct LockOrientationResultHolder {
    LockOrientationResultHolder() : succeeded_(false), failed_(false) {}

    bool succeeded_;
    bool failed_;
    blink::WebLockOrientationError error_;
  };

  explicit MockLockOrientationCallback(LockOrientationResultHolder* results)
      : results_(results) {}

  void OnSuccess() override { results_->succeeded_ = true; }

  void OnError(blink::WebLockOrientationError error) override {
    results_->failed_ = true;
    results_->error_ = error;
  }

 private:
  LockOrientationResultHolder* results_;
};

class ScreenOrientationControllerImplTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    ScreenOrientationControllerImpl::ProvideTo(GetFrame());
    mojo::AssociatedRemote<device::mojom::blink::ScreenOrientation>
        screen_orientation;
    ignore_result(
        screen_orientation.BindNewEndpointAndPassDedicatedReceiverForTesting());
    Controller()->SetScreenOrientationAssociatedRemoteForTests(
        std::move(screen_orientation));
  }

  void TearDown() override {
    Controller()->SetScreenOrientationAssociatedRemoteForTests(
        mojo::AssociatedRemote<device::mojom::blink::ScreenOrientation>());
  }

  ScreenOrientationControllerImpl* Controller() {
    return ScreenOrientationControllerImpl::From(GetFrame());
  }

  void LockOrientation(
      blink::WebScreenOrientationLockType orientation,
      std::unique_ptr<blink::WebLockOrientationCallback> callback) {
    Controller()->lock(orientation, std::move(callback));
  }

  void UnlockOrientation() { Controller()->unlock(); }

  int GetRequestId() { return Controller()->GetRequestIdForTests(); }

  void RunLockResultCallback(int request_id, LockResult result) {
    Controller()->OnLockOrientationResult(request_id, result);
  }
};

// Test that calling lockOrientation() followed by unlockOrientation() cancel
// the lockOrientation().
TEST_F(ScreenOrientationControllerImplTest, CancelPending_Unlocking) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;

  LockOrientation(
      blink::kWebScreenOrientationLockPortraitPrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results));
  UnlockOrientation();

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::kWebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that calling lockOrientation() twice cancel the first lockOrientation().
TEST_F(ScreenOrientationControllerImplTest, CancelPending_DoubleLock) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  // We create the object to prevent leaks but never actually use it.
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      blink::kWebScreenOrientationLockPortraitPrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results));

  LockOrientation(
      blink::kWebScreenOrientationLockPortraitPrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results2));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::kWebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that when a LockError message is received, the request is set as failed
// with the correct values.
TEST_F(ScreenOrientationControllerImplTest, LockRequest_Error) {
  HashMap<LockResult, blink::WebLockOrientationError, WTF::IntHash<LockResult>>
      errors;
  errors.insert(LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE,
                blink::kWebLockOrientationErrorNotAvailable);
  errors.insert(
      LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED,
      blink::kWebLockOrientationErrorFullscreenRequired);
  errors.insert(LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED,
                blink::kWebLockOrientationErrorCanceled);

  for (auto it = errors.begin(); it != errors.end(); ++it) {
    MockLockOrientationCallback::LockOrientationResultHolder callback_results;
    LockOrientation(
        blink::kWebScreenOrientationLockPortraitPrimary,
        std::make_unique<MockLockOrientationCallback>(&callback_results));
    RunLockResultCallback(GetRequestId(), it->key);
    EXPECT_FALSE(callback_results.succeeded_);
    EXPECT_TRUE(callback_results.failed_);
    EXPECT_EQ(it->value, callback_results.error_);
  }
}

// Test that when a LockSuccess message is received, the request is set as
// succeeded.
TEST_F(ScreenOrientationControllerImplTest, LockRequest_Success) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(
      blink::kWebScreenOrientationLockPortraitPrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results));

  RunLockResultCallback(GetRequestId(),
                        LockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);

  EXPECT_TRUE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);
}

// Test the following scenario:
// - request1 is received by the delegate;
// - request2 is received by the delegate;
// - request1 is rejected;
// - request1 success response is received.
// Expected: request1 is still rejected, request2 has not been set as succeeded.
TEST_F(ScreenOrientationControllerImplTest, RaceScenario) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results1;
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      blink::kWebScreenOrientationLockPortraitPrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results1));
  int request_id1 = GetRequestId();

  LockOrientation(
      blink::kWebScreenOrientationLockLandscapePrimary,
      std::make_unique<MockLockOrientationCallback>(&callback_results2));

  // callback_results1 must be rejected, tested in CancelPending_DoubleLock.

  RunLockResultCallback(request_id1,
                        LockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);

  // First request is still rejected.
  EXPECT_FALSE(callback_results1.succeeded_);
  EXPECT_TRUE(callback_results1.failed_);
  EXPECT_EQ(blink::kWebLockOrientationErrorCanceled, callback_results1.error_);

  // Second request is still pending.
  EXPECT_FALSE(callback_results2.succeeded_);
  EXPECT_FALSE(callback_results2.failed_);
}

}  // namespace blink
