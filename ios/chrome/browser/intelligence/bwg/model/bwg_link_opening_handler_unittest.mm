// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kTestURL[] = "https://www.example.com/";

}  // namespace

// Test fixture for BWGLinkOpeningHandler.
class BwgLinkOpeningHandlerTest : public PlatformTest {
 protected:
  BwgLinkOpeningHandlerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Create the notifier and inject the fake URL loading browser agent.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    // Get the fake URL loader from the browser.
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    // Initialize the link opening handler with the fake loader.
    link_opening_handler_ =
        [[BWGLinkOpeningHandler alloc] initWithURLLoader:url_loader_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  BWGLinkOpeningHandler* link_opening_handler_;
};

// Tests that openURLInNewTab calls Load with the correct URL.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTab) {
  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify the URL was passed to the loader.
  EXPECT_EQ(kTestURL, url_loader_->last_params.web_params.url.spec());
}

// Tests that openURLInNewTab records the appropriate user action.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabRecordsUserAction) {
  base::UserActionTester user_action_tester;

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify that the URL opened user action was recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileGeminiURLOpened"));
}

// Tests that openURLInNewTab handles empty URL gracefully.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabWithEmptyURL) {
  [link_opening_handler_ openURLInNewTab:@""];

  // The URL will be invalid but the fake loader handles it gracefully.
  EXPECT_FALSE(url_loader_->last_params.web_params.url.is_valid());
}
