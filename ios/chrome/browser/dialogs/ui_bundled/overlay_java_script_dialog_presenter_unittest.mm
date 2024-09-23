// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dialogs/ui_bundled/overlay_java_script_dialog_presenter.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_confirm_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test fixture for OverlayJavaScriptDialogPresenter.
class OverlayJavaScriptDialogPresenterTest : public PlatformTest {
 protected:
  OverlayJavaScriptDialogPresenterTest() : url_("http://chromium.test") {
    OverlayRequestQueue::CreateForWebState(&web_state_);
  }

  const GURL url_;
  web::FakeWebState web_state_;
  OverlayJavaScriptDialogPresenter presenter_;
};

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptAlertRequest.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunJavaScriptAlertDialog) {
  presenter_.RunJavaScriptAlertDialog(&web_state_, url_, @"",
                                      base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->GetConfig<JavaScriptAlertDialogRequest>());
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptConfirmationOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunJavaScriptConfirmDialog) {
  presenter_.RunJavaScriptConfirmDialog(&web_state_, url_, @"",
                                        base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->GetConfig<JavaScriptConfirmDialogRequest>());
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptPromptOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunJavaScriptPromptDialog) {
  presenter_.RunJavaScriptPromptDialog(&web_state_, url_, @"", @"",
                                       base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->GetConfig<JavaScriptPromptDialogRequest>());
}

// Tests that the presenter removes all requests from the queue when
// CancelDialogs() is called.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunJavaScriptDialogCancelDialogs) {
  presenter_.RunJavaScriptAlertDialog(&web_state_, url_, @"",
                                      base::DoNothing());
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue->front_request());

  // Cancel the requests and verify that the queue is emptied.
  presenter_.CancelDialogs(&web_state_);
  ASSERT_FALSE(queue->front_request());
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptAlertRequest.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunAlert) {
  presenter_.RunJavaScriptAlertDialog(&web_state_, url_, @"",
                                      base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  JavaScriptAlertDialogRequest* dialog_request =
      request->GetConfig<JavaScriptAlertDialogRequest>();
  ASSERT_TRUE(dialog_request);
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptConfirmationOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunConfirmation) {
  presenter_.RunJavaScriptConfirmDialog(&web_state_, url_, @"",
                                        base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  JavaScriptConfirmDialogRequest* dialog_request =
      request->GetConfig<JavaScriptConfirmDialogRequest>();
  ASSERT_TRUE(dialog_request);
}

// Tests that the presenter adds an OverlayRequest configured with a
// JavaScriptPromptOverlayRequestConfig.
TEST_F(OverlayJavaScriptDialogPresenterTest, RunPrompt) {
  presenter_.RunJavaScriptPromptDialog(&web_state_, url_, @"", @"",
                                       base::DoNothing());

  // Verify that an alert OverlayRequest is added to the queue.
  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                &web_state_, OverlayModality::kWebContentArea)
                                ->front_request();
  ASSERT_TRUE(request);
  JavaScriptPromptDialogRequest* dialog_request =
      request->GetConfig<JavaScriptPromptDialogRequest>();
  ASSERT_TRUE(dialog_request);
}

// Tests that the presenter removes all requests from the queue when
// CancelDialogs() is called.
TEST_F(OverlayJavaScriptDialogPresenterTest, CancelDialogs) {
  presenter_.RunJavaScriptAlertDialog(&web_state_, url_, @"",
                                      base::DoNothing());
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue->front_request());

  // Cancel the requests and verify that the queue is emptied.
  presenter_.CancelDialogs(&web_state_);
  ASSERT_FALSE(queue->front_request());
}
