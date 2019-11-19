// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/default_overlay_request_cancel_handler.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for DefaultOverlayRequestCancelHandler.
class DefaultOverlayRequestCancelHandlerTest : public PlatformTest {
 public:
  DefaultOverlayRequestCancelHandlerTest() : PlatformTest() {
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
    queue()->AddRequest(std::move(request));
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kWebContentArea);
  }
  web::TestWebState* web_state() { return &web_state_; }

 private:
  web::TestWebState web_state_;
};

// Tests that the request is removed from the queue for committed, document-
// changing navigations.
TEST_F(DefaultOverlayRequestCancelHandlerTest, CancelForNavigations) {
  // Simulate a navigation.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state()->OnNavigationFinished(&context);

  // Verify that the queue is empty.
  EXPECT_FALSE(queue()->front_request());
}

// Tests that the request is removed from the queue for renderer crashes.
TEST_F(DefaultOverlayRequestCancelHandlerTest, CancelForRendererCrashes) {
  web_state()->OnRenderProcessGone();

  // Verify that the queue is empty.
  EXPECT_FALSE(queue()->front_request());
}
