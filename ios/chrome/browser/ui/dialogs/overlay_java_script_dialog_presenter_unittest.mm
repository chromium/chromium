// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/overlay_java_script_dialog_presenter.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for OverlayJavaScriptDialogPresenter.
class OverlayJavaScriptDialogPresenterTest : public PlatformTest {
 protected:
  OverlayJavaScriptDialogPresenterTest() : url_("http://chromium.test") {}

  const GURL url_;
  web::TestWebState web_state_;
  OverlayJavaScriptDialogPresenter presenter_;
};

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptAlertOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunAlert) {
  web::DialogClosedCallback callback =
      base::BindOnce(^(bool success, NSString* user_input){
      });
  presenter_.RunJavaScriptDialog(&web_state_, url_,
                                 web::JAVASCRIPT_DIALOG_TYPE_ALERT, @"", @"",
                                 std::move(callback));

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<JavaScriptAlertOverlayRequestConfig>());
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptConfirmationOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunConfirmation) {
  web::DialogClosedCallback callback =
      base::BindOnce(^(bool success, NSString* user_input){
      });
  presenter_.RunJavaScriptDialog(&web_state_, url_,
                                 web::JAVASCRIPT_DIALOG_TYPE_CONFIRM, @"", @"",
                                 std::move(callback));

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<JavaScriptConfirmationOverlayRequestConfig>());
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptPromptOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunPrompt) {
  web::DialogClosedCallback callback =
      base::BindOnce(^(bool success, NSString* user_input){
      });
  presenter_.RunJavaScriptDialog(&web_state_, url_,
                                 web::JAVASCRIPT_DIALOG_TYPE_PROMPT, @"", @"",
                                 std::move(callback));

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<JavaScriptPromptOverlayRequestConfig>());
}

// Tests that the presenter removes all requests from the queue when
// CancelDialogs() is called.
TEST_F(OverlayJavaScriptDialogPresenterTest, CancelDialogs) {
  web::DialogClosedCallback callback =
      base::BindOnce(^(bool success, NSString* user_input){
      });
  presenter_.RunJavaScriptDialog(&web_state_, url_,
                                 web::JAVASCRIPT_DIALOG_TYPE_ALERT, @"", @"",
                                 std::move(callback));
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue->front_request());

  // Cancel the requests and verify that the queue is emptied.
  presenter_.CancelDialogs(&web_state_);
  ASSERT_FALSE(queue->front_request());
}
