// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/ui/java_script_dialog_presenter.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for WebStateDelegateTabHelper.
class WebStateDelegateTabHelperTest : public PlatformTest {
 public:
  WebStateDelegateTabHelperTest() {
    WebStateDelegateTabHelper::CreateForWebState(&web_state_);
  }
  ~WebStateDelegateTabHelperTest() override = default;

  web::WebState* web_state() { return &web_state_; }
  web::WebStateDelegate* delegate() {
    return WebStateDelegateTabHelper::FromWebState(web_state());
  }

 protected:
  web::TestWebState web_state_;
};

// Tests that OnAuthRequired() adds an HTTP authentication overlay request to
// the WebState's OverlayRequestQueue at OverlayModality::kWebContentArea.
TEST_F(WebStateDelegateTabHelperTest, OnAuthRequired) {
  NSURLProtectionSpace* protection_space =
      [[NSURLProtectionSpace alloc] initWithProxyHost:@"http://chromium.test"
                                                 port:0
                                                 type:nil
                                                realm:nil
                                 authenticationMethod:nil];
  NSURLCredential* credential =
      [[NSURLCredential alloc] initWithUser:@""
                                   password:@""
                                persistence:NSURLCredentialPersistenceNone];
  web::WebStateDelegate::AuthCallback callback =
      base::BindRepeating(^(NSString* user, NSString* password){
      });
  delegate()->OnAuthRequired(web_state(), protection_space, credential,
                             callback);

  // Verify that an HTTP auth overlay request has been created for the WebState.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state(), OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  EXPECT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<HTTPAuthOverlayRequestConfig>());
}

// Tests that GetJavaScriptDialogPresenter() returns an overlay-based JavaScript
// dialog presenter.
TEST_F(WebStateDelegateTabHelperTest, GetJavaScriptDialogPresenter) {
  // Verify that the delegate returns a non-null presenter.
  web::JavaScriptDialogPresenter* presenter =
      delegate()->GetJavaScriptDialogPresenter(web_state());
  EXPECT_TRUE(presenter);

  // Present a JavaScript alert.
  GURL kOriginUrl("http://chromium.test");
  web::DialogClosedCallback callback =
      base::BindOnce(^(bool success, NSString* user_input){
      });
  presenter->RunJavaScriptDialog(web_state(), kOriginUrl,
                                 web::JAVASCRIPT_DIALOG_TYPE_ALERT, @"", @"",
                                 std::move(callback));

  // Verify that JavaScript alert OverlayRequest has been added to the
  // WebState's queue.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state(), OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue);
  OverlayRequest* request = queue->front_request();
  EXPECT_TRUE(request);
  EXPECT_TRUE(request->GetConfig<JavaScriptAlertOverlayRequestConfig>());
}
