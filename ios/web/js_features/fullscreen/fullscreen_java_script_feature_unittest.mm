// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/fullscreen/fullscreen_java_script_feature.h"

#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest_mac.h"

namespace web {

class FullscreenJavaScriptFeatureTest : public WebTestWithWebState {
 protected:
  FullscreenJavaScriptFeatureTest() = default;
  ~FullscreenJavaScriptFeatureTest() override = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
  }
};

// Tests that a page with viewport-fit=cover correctly propagates this state
// to CRWWebControllerContainerView.
TEST_F(FullscreenJavaScriptFeatureTest, ViewportFitCoverPropagates) {
  NSString* html = @"<html><head>"
                    "<meta name=\"viewport\" content=\"viewport-fit=cover\" />"
                    "</head><body></body></html>";
  LoadHtml(html);
  ASSERT_TRUE(WaitUntilLoaded());

  CRWWebController* web_controller =
      web::WebStateImpl::FromWebState(web_state())->GetWebController();
  ASSERT_TRUE(web_controller.isCover);
}

// Tests that a page with viewport-fit=auto correctly propagates this state
// to CRWWebControllerContainerView.
TEST_F(FullscreenJavaScriptFeatureTest, ViewportFitAutoPropagates) {
  NSString* html = @"<html><head>"
                    "<meta name=\"viewport\" content=\"viewport-fit=auto\" />"
                    "</head><body></body></html>";
  LoadHtml(html);
  ASSERT_TRUE(WaitUntilLoaded());

  CRWWebController* web_controller =
      web::WebStateImpl::FromWebState(web_state())->GetWebController();
  ASSERT_FALSE(web_controller.isCover);
}

// Tests that a page with no viewport-fit value correctly propagates this state
// to CRWWebControllerContainerView.
TEST_F(FullscreenJavaScriptFeatureTest, NoViewportFitPropagates) {
  NSString* html = @"<html><head>"
                    "</head><body></body></html>";
  LoadHtml(html);
  ASSERT_TRUE(WaitUntilLoaded());

  CRWWebController* web_controller =
      web::WebStateImpl::FromWebState(web_state())->GetWebController();
  ASSERT_FALSE(web_controller.isCover);
}

}  // namespace web
