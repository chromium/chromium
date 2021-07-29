// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kURL1[] = "https://www.some.url.com";

class TabModelTest : public PlatformTest {
 public:
  TabModelTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list_.get());
    // Web usage is disabled during these tests.
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    web_usage_enabler_ =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    web_usage_enabler_->SetWebUsageEnabled(false);

    session_window_ = [[SessionWindowIOS alloc] init];

    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());

    agent_ = TabInsertionBrowserAgent::FromBrowser(browser_.get());
    session_service_ = [[TestSessionService alloc] init];
    // Create session restoration agent with just a dummy session
    // service so the async state saving doesn't trigger unless actually
    // wanted.
    SessionRestorationBrowserAgent::CreateForBrowser(browser_.get(),
                                                     session_service_);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);
    SetTabModel(CreateTabModel(nil));
  }

  ~TabModelTest() override = default;

  void TearDown() override {
    SetTabModel(nil);
    PlatformTest::TearDown();
  }

  void SetTabModel(TabModel* tab_model) {
    if (tab_model_) {
      @autoreleasepool {
        [tab_model_ disconnect];
        tab_model_ = nil;
      }
    }

    tab_model_ = tab_model;
  }

  TabModel* CreateTabModel(SessionWindowIOS* session_window) {
    TabModel* tab_model([[TabModel alloc] initWithBrowser:browser_.get()]);
    return tab_model;
  }

  const web::NavigationManager::WebLoadParams Params(GURL url) {
    return Params(url, ui::PAGE_TRANSITION_TYPED);
  }

  const web::NavigationManager::WebLoadParams Params(
      GURL url,
      ui::PageTransition transition) {
    web::NavigationManager::WebLoadParams loadParams(url);
    loadParams.referrer = web::Referrer();
    loadParams.transition_type = transition;
    return loadParams;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<Browser> browser_;
  SessionWindowIOS* session_window_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TabInsertionBrowserAgent* agent_;
  TestSessionService* session_service_;
  WebUsageEnablerBrowserAgent* web_usage_enabler_;
  TabModel* tab_model_;
};

TEST_F(TabModelTest, InsertWithSessionController) {
  EXPECT_EQ(web_state_list_->count(), 0);

  web::WebState* new_web_state =
      agent_->InsertWebState(Params(GURL(kURL1)),
                             /*parent=*/nil,
                             /*opened_by_dom=*/false,
                             /*index=*/web_state_list_->count(),
                             /*in_background=*/false,
                             /*inherit_opener=*/false);

  EXPECT_EQ(web_state_list_->count(), 1);
  EXPECT_EQ(new_web_state, web_state_list_->GetWebStateAt(0));
  web_state_list_->ActivateWebStateAt(0);
  web::WebState* current_web_state = web_state_list_->GetActiveWebState();
  EXPECT_TRUE(current_web_state);
}

}  // anonymous namespace
