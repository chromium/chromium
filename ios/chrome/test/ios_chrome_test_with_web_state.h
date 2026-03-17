// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_TEST_WITH_WEB_STATE_H_
#define IOS_CHROME_TEST_IOS_CHROME_TEST_WITH_WEB_STATE_H_

#import <memory>
#import <string>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace web {
class JavaScriptFeature;
class WebFrame;
class WebState;
}  // namespace web

// Test fixture for //ios/chrome tests that need a WebState.
//
// This is an //ios/chrome version of WebTestWithWebState, since that fixture
// uses a web::BrowserState* as the BrowserState and breaks //ios/chrome
// invariants about ProfileIOS.See //ios/web/public/test:test_fixture for more
// context.
class IOSChromeTestWithWebState : public PlatformTest {
 public:
  IOSChromeTestWithWebState();
  ~IOSChromeTestWithWebState() override;

 protected:
  // Loads an HTML page into the WebState.
  void LoadHtml(NSString* html);
  void LoadHtml(std::string html);

  web::WebState* web_state() const { return web_state_.get(); }
  web::FakeWebClient* fake_web_client() { return &web_client_; }

  // Gets the main frame for the WebState once it's loaded.
  web::WebFrame* WaitForMainFrame(web::JavaScriptFeature* feature);

  // Waits until the WebState's frames manager reports the expected number of
  // frames.
  [[nodiscard]] bool WaitForFrameCount(web::JavaScriptFeature* feature,
                                       size_t expected_count);

  web::WebTaskEnvironment task_environment_;
  web::FakeWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_TEST_WITH_WEB_STATE_H_
