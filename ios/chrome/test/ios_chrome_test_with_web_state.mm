// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ios_chrome_test_with_web_state.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

IOSChromeTestWithWebState::IOSChromeTestWithWebState() {
  web::SetWebClient(&web_client_);
  profile_ = TestProfileIOS::Builder().Build();
  web::WebState::CreateParams params(profile_.get());
  web_state_ = web::WebState::Create(params);
}

IOSChromeTestWithWebState::~IOSChromeTestWithWebState() = default;

void IOSChromeTestWithWebState::LoadHtml(NSString* html) {
  web::test::LoadHtml(html, web_state_.get());
}

void IOSChromeTestWithWebState::LoadHtml(std::string html) {
  web::test::LoadHtml(base::SysUTF8ToNSString(html), web_state_.get());
}

web::WebFrame* IOSChromeTestWithWebState::WaitForMainFrame(
    web::JavaScriptFeature* feature) {
  __block web::WebFrame* main_frame = nullptr;
  web::WebFramesManager* frames_manager =
      feature->GetWebFramesManager(web_state());
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        main_frame = frames_manager->GetMainWebFrame();
        return main_frame != nullptr;
      }));
  return main_frame;
}

bool IOSChromeTestWithWebState::WaitForFrameCount(
    web::JavaScriptFeature* feature,
    size_t expected_count) {
  web::WebFramesManager* frames_manager =
      feature->GetWebFramesManager(web_state());
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return frames_manager->GetAllWebFrames().size() == expected_count;
      });
}
