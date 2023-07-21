// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
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
  raw_ptr<LockOrientationResultHolder> results_;
};

class ScreenOrientationControllerTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    HeapMojoAssociatedRemote<device::mojom::blink::ScreenOrientation>
        screen_orientation(GetFrame().DomWindow());
    std::ignore = screen_orientation.BindNewEndpointAndPassDedicatedReceiver();
    Controller()->SetScreenOrientationAssociatedRemoteForTests(
        std::move(screen_orientation));
  }

  void TearDown() override {
    HeapMojoAssociatedRemote<device::mojom::blink::ScreenOrientation>
        screen_orientation(GetFrame().DomWindow());
    Controller()->SetScreenOrientationAssociatedRemoteForTests(
        std::move(screen_orientation));
  }

  ScreenOrientationController* Controller() {
    return ScreenOrientationController::From(*GetFrame().DomWindow());
  }

  void LockOrientation(
      device::mojom::ScreenOrientationLockType orientation,
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
TEST_F(ScreenOrientationControllerTest, CancelPending_Unlocking) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;

  LockOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
      std::make_unique<MockLockOrientationCallback>(&callback_results));
  UnlockOrientation();

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::kWebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that calling lockOrientation() twice cancel the first lockOrientation().
TEST_F(ScreenOrientationControllerTest, CancelPending_DoubleLock) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  // We create the object to prevent leaks but never actually use it.
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
      std::make_unique<MockLockOrientationCallback>(&callback_results));

  LockOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
      std::make_unique<MockLockOrientationCallback>(&callback_results2));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::kWebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that when a LockError message is received, the request is set as failed
// with the correct values.
TEST_F(ScreenOrientationControllerTest, LockRequest_Error) {
  HashMap<LockResult, blink::WebLockOrientationError> errors;
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
        device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
        std::make_unique<MockLockOrientationCallback>(&callback_results));
    RunLockResultCallback(GetRequestId(), it->key);
    EXPECT_FALSE(callback_results.succeeded_);
    EXPECT_TRUE(callback_results.failed_);
    EXPECT_EQ(it->value, callback_results.error_);
  }
}

// Test that when a LockSuccess message is received, the request is set as
// succeeded.
TEST_F(ScreenOrientationControllerTest, LockRequest_Success) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
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
TEST_F(ScreenOrientationControllerTest, RaceScenario) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results1;
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
      std::make_unique<MockLockOrientationCallback>(&callback_results1));
  int request_id1 = GetRequestId();

  LockOrientation(
      device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY,
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

class ScreenInfoWebFrameWidget : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  explicit ScreenInfoWebFrameWidget(Args&&... args)
      : frame_test_helpers::TestWebFrameWidget(std::forward<Args>(args)...) {
    screen_info_.orientation_angle = 1234;
  }
  ~ScreenInfoWebFrameWidget() override = default;

  // frame_test_helpers::TestWebFrameWidget overrides.
  display::ScreenInfo GetInitialScreenInfo() override { return screen_info_; }

 private:
  display::ScreenInfo screen_info_;
};

TEST_F(ScreenOrientationControllerTest, PageVisibilityCrash) {
  std::string base_url("http://internal.test/");
  std::string test_url("single_iframe.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(test_url));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));

  frame_test_helpers::CreateTestWebFrameWidgetCallback create_widget_callback =
      WTF::BindRepeating(
          &frame_test_helpers::WebViewHelper::CreateTestWebFrameWidget<
              ScreenInfoWebFrameWidget>);
  frame_test_helpers::WebViewHelper web_view_helper(create_widget_callback);
  web_view_helper.InitializeAndLoad(base_url + test_url, nullptr, nullptr);

  Page* page = web_view_helper.GetWebView()->GetPage();
  LocalFrame* frame = To<LocalFrame>(page->MainFrame());

  // Fully set up on an orientation and a controller in the main frame, but not
  // the iframe. Prepare an orientation change, then toggle visibility. When
  // set to visible, propagating the orientation change events shouldn't crash
  // just because the ScreenOrientationController in the iframe was never
  // referenced before this.
  ScreenOrientation::Create(frame->DomWindow());
  page->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden, false);
  web_view_helper.LocalMainFrame()->SendOrientationChangeEvent();
  page->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible, false);

  // When the iframe's orientation is initialized, it should be properly synced.
  auto* child_orientation = ScreenOrientation::Create(
      To<LocalFrame>(frame->Tree().FirstChild())->DomWindow());
  EXPECT_EQ(child_orientation->angle(), 1234);

  url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  web_view_helper.Reset();
}

TEST_F(ScreenOrientationControllerTest,
       OrientationChangePropagationToGrandchild) {
  std::string base_url("http://internal.test/");
  std::string test_url("page_with_grandchild.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(test_url));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("single_iframe.html"));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url + test_url, nullptr, nullptr);

  Page* page = web_view_helper.GetWebView()->GetPage();
  LocalFrame* frame = To<LocalFrame>(page->MainFrame());

  // Fully set up on an orientation and a controller in the main frame and
  // the grandchild, but not the child.
  ScreenOrientation::Create(frame->DomWindow());
  Frame* grandchild = frame->Tree().FirstChild()->Tree().FirstChild();
  auto* grandchild_orientation =
      ScreenOrientation::Create(To<LocalFrame>(grandchild)->DomWindow());

  // Update the screen info and ensure it propagated to the grandchild.
  display::ScreenInfos screen_infos((display::ScreenInfo()));
  screen_infos.mutable_current().orientation_angle = 90;
  auto* web_frame_widget_base =
      static_cast<WebFrameWidgetImpl*>(frame->GetWidgetForLocalRoot());
  web_frame_widget_base->UpdateScreenInfo(screen_infos);
  EXPECT_EQ(grandchild_orientation->angle(), 90);

  url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  web_view_helper.Reset();
}

}  // namespace blink
