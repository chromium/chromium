// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ios_chrome_test_with_web_state.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

IOSChromeTestWithWebState::IOSChromeTestWithWebState(
    WebClientMode web_client_mode) {
  switch (web_client_mode) {
    case WebClientMode::kChromeWebClient:
      web_client_.reset(new ChromeWebClient());
      break;
    case WebClientMode::kFakeWebClient:
      web_client_.reset(new web::FakeWebClient());
      break;
  }
  web::SetWebClient(web_client_.get());
  profile_ = TestProfileIOS::Builder().Build();
  web::WebState::CreateParams params(profile_.get());
  web_state_ = web::WebState::Create(params);
}

IOSChromeTestWithWebState::~IOSChromeTestWithWebState() {
  web_state_.reset();
  profile_.reset();
  web::SetWebClient(nullptr);
}

void IOSChromeTestWithWebState::LoadHtml(NSString* html) {
  web::test::LoadHtml(html, web_state_.get());
}

void IOSChromeTestWithWebState::LoadHtml(std::string html) {
  web::test::LoadHtml(base::SysUTF8ToNSString(html), web_state_.get());
}

bool IOSChromeTestWithWebState::LoadUrl(const GURL& url) {
  web::test::LoadUrl(web_state_.get(), url);
  return web::test::WaitForPageToFinishLoading(web_state());
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
