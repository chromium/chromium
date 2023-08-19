// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/child_frame_registration_java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_constants.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Text fixture to test ChildFrameRegistrationJavaScriptFeature.
class ChildFrameRegistrationJavaScriptFeatureTest : public PlatformTest {
 protected:
  ChildFrameRegistrationJavaScriptFeatureTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillAcrossIframesIos);

    PlatformTest::SetUp();

    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  // Loads the given HTML and initializes the Autofill JS scripts.
  void LoadHtml(NSString* html) {
    web::test::LoadHtml(html, web_state());

    __block web::WebFrame* main_frame = nullptr;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      main_frame = main_web_frame();
      return main_frame != nullptr;
    }));
    ASSERT_TRUE(main_frame);
  }

  web::WebFrame* main_web_frame() {
    web::WebFramesManager* frames_manager =
        feature()->GetWebFramesManager(web_state());

    return frames_manager->GetMainWebFrame();
  }

  autofill::ChildFrameRegistrationJavaScriptFeature* feature() {
    return autofill::ChildFrameRegistrationJavaScriptFeature::GetInstance();
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;

  base::test::ScopedFeatureList feature_list_;
};

// Tests that the feature is created and injected, and that frame IDs are
// successfully deserialized into {Local, Remote}FrameTokens.
TEST_F(ChildFrameRegistrationJavaScriptFeatureTest, SmokeTest) {
  LoadHtml(@"<body></body>");
  ASSERT_TRUE(feature());

  // Wait until something is in the map, and check that it's valid.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return !feature()->lookup_map.empty();
  }));
  auto kv_pair = *feature()->lookup_map.begin();

  // `first` is the LocalFrameToken, `second` is the RemoteFrameToken. Ensure
  // neither is empty (i.e., uninitialized).
  EXPECT_FALSE(kv_pair.first.is_empty());
  EXPECT_FALSE(kv_pair.second.is_empty());
}

}  // namespace
