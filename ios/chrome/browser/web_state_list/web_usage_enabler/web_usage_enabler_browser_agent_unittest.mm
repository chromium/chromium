// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL to load in WebStates.
const char kURL[] = "https://chromium.org";
}

class WebUsageEnablerBrowserAgentTest : public PlatformTest {
 public:
  WebUsageEnablerBrowserAgentTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
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
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
  WebUsageEnablerBrowserAgent* enabler_;

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
    web_state_list_->InsertWebState(WebStateList::kInvalidIndex,
                                    CreateWebState(url),
                                    WebStateList::INSERT_NO_FLAGS, opener);
  }

  bool InitialLoadTriggeredForLastWebState() {
    if (web_state_list_->count() <= 0)
      return false;
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

  // Insert with FORCE_INDEX to not activate and not trigger a load.
  web_state_list_->InsertWebState(0, CreateWebState(kURL),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  EXPECT_FALSE(InitialLoadTriggeredForLastWebState());

  // Insert without FORCE_INDEX and verify LoadIfNecessary() was called.
  web_state_list_->InsertWebState(
      0, CreateWebState(kURL), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  EXPECT_TRUE(InitialLoadTriggeredForLastWebState());
}
