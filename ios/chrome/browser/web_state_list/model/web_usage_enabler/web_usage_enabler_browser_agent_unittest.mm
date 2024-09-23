// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// URL to load in WebStates.
const char kURL[] = "https://chromium.org";
}  // namespace

class WebUsageEnablerBrowserAgentTest : public PlatformTest {
 public:
  WebUsageEnablerBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    enabler_ = WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler_->SetWebUsageEnabled(false);
  }

  WebUsageEnablerBrowserAgentTest(const WebUsageEnablerBrowserAgentTest&) =
      delete;
  WebUsageEnablerBrowserAgentTest& operator=(
      const WebUsageEnablerBrowserAgentTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<WebUsageEnablerBrowserAgent> enabler_;

  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(GURL(url));
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    return test_web_state;
  }

  void AppendNewWebState(const char* url) {
    AppendNewWebState(url, WebStateOpener());
  }

  void AppendNewWebState(const char* url, WebStateOpener opener) {
    web_state_list_->InsertWebState(
        CreateWebState(url),
        WebStateList::InsertionParams::Automatic().WithOpener(opener));
  }

  bool InitialLoadTriggeredForLastWebState() {
    if (web_state_list_->count() <= 0) {
      return false;
    }
    web::WebState* last_web_state =
        web_state_list_->GetWebStateAt(web_state_list_->count() - 1);
    web::FakeNavigationManager* navigation_manager =
        static_cast<web::FakeNavigationManager*>(
            last_web_state->GetNavigationManager());
    return navigation_manager->LoadIfNecessaryWasCalled();
  }

  void VerifyWebUsageEnabled(bool enabled) {
    for (int index = 0; index < web_state_list_->count(); ++index) {
      web::WebState* web_state = web_state_list_->GetWebStateAt(index);
      EXPECT_EQ(web_state->IsWebUsageEnabled(), enabled);
    }
  }
};

// Tests that calling SetWebUsageEnabled() updates web usage enabled state for
// WebStates already added to the WebStateList as well as those that are added
// afterward.
TEST_F(WebUsageEnablerBrowserAgentTest, EnableWebUsage) {
  // Add a WebState with usage disabled.
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(false);
  // Enable web usage and add another WebState.  All WebStates in the list
  // should have usage enabled, including the most recently added.
  enabler_->SetWebUsageEnabled(true);
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(true);
  // Disable web usage and add another WebState.  All WebStates in the list
  // should have usage disabled, including the most recently added.
  enabler_->SetWebUsageEnabled(false);
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(false);
}

// Tests that TriggersInitialLoad() correctly controls whether the initial load
// of newly added WebStates from being kicked off.
TEST_F(WebUsageEnablerBrowserAgentTest, DisableInitialLoad) {
  enabler_->SetWebUsageEnabled(true);

  // Insert with AtIndex and without activating to not trigger a load.
  web_state_list_->InsertWebState(
      CreateWebState(kURL),
      WebStateList::InsertionParams::AtIndex(0).Activate(false));
  EXPECT_FALSE(InitialLoadTriggeredForLastWebState());

  // Insert with activating and verify LoadIfNecessary() was called.
  web_state_list_->InsertWebState(
      CreateWebState(kURL),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(InitialLoadTriggeredForLastWebState());
}
