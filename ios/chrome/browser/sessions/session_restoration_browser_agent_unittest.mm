// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kURL1[] = "https://www.some.url.com";
const char kURL2[] = "https://www.some.url2.com";

class TestRestorationObserver : public SessionRestorationObserver {
 public:
  bool restore_started() { return restore_started_; }
  int restored_web_states_count() { return restored_web_states_count_; }

 private:
  void WillStartSessionRestoration() override { restore_started_ = true; }
  void SessionRestorationFinished(
      const std::vector<web::WebState*>& restored_web_states) override {
    restored_web_states_count_ = restored_web_states.size();
  }

  bool restore_started_ = false;
  int restored_web_states_count_ = -1;
};

class SessionRestorationBrowserAgentTest : public PlatformTest {
 public:
  SessionRestorationBrowserAgentTest()
      : web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())),
        test_session_service_([[TestSessionService alloc] init]) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    // This test requires that some TabHelpers are attached to the WebStates, so
    // it needs to use a WebStateList with the full BrowserWebStateListDelegate,
    // rather than the TestWebStateList delegate used in the default TestBrowser
    // constructor.
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list_.get());
    // Web usage is disabled during these tests.
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    web_usage_enabler_ =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    web_usage_enabler_->SetWebUsageEnabled(false);

    session_identifier_ = [[NSUUID UUID] UUIDString];
    SessionRestorationBrowserAgent::CreateForBrowser(browser_.get(),
                                                     test_session_service_);
    session_restoration_agent_ =
        SessionRestorationBrowserAgent::FromBrowser(browser_.get());
    session_restoration_agent_->SetSessionID(session_identifier_);
  }

  ~SessionRestorationBrowserAgentTest() override = default;

  void TearDown() override {
    @autoreleasepool {
      web_state_list_->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
    }
    PlatformTest::TearDown();
  }

  NSString* session_id() { return session_identifier_; }

 protected:
  // Creates a session window with |sessions_count| and mark the
  // |selected_index| entry as selected.
  SessionWindowIOS* CreateSessionWindow(int sessions_count,
                                        int selected_index) {
    NSMutableArray<CRWSessionStorage*>* sessions = [NSMutableArray array];
    for (int i = 0; i < sessions_count; i++) {
      CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
      session_storage.lastCommittedItemIndex = -1;
      [sessions addObject:session_storage];
    }
    return [[SessionWindowIOS alloc] initWithSessions:sessions
                                        selectedIndex:selected_index];
  }

  // Creates a WebState with the given parameters and insert it in the
  // |web_state_list_|.
  web::WebState* InsertNewWebState(const GURL& url,
                                   web::WebState* parent,
                                   int index,
                                   bool background) {
    web::NavigationManager::WebLoadParams load_params(url);
    load_params.transition_type = ui::PAGE_TRANSITION_TYPED;

    web::WebState::CreateParams create_params(chrome_browser_state_.get());
    create_params.created_with_opener = false;

    std::unique_ptr<web::WebState> web_state =
        web::WebState::Create(create_params);
    web_state->GetNavigationManager()->LoadURLWithParams(load_params);

    int insertion_flags = WebStateList::INSERT_FORCE_INDEX;
    if (!background)
      insertion_flags |= WebStateList::INSERT_ACTIVATE;
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    insertion_flags, WebStateOpener(parent));
    return web_state_list_->GetWebStateAt(index);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;

  __strong NSString* session_identifier_ = nil;
  TestSessionService* test_session_service_;
  WebUsageEnablerBrowserAgent* web_usage_enabler_;
  SessionRestorationBrowserAgent* session_restoration_agent_;
};

// Tests that restoring a session works correctly on empty WebStateList.
TEST_F(SessionRestorationBrowserAgentTest, RestoreSessionOnEmptyWebStateList) {
  SessionWindowIOS* window(
      CreateSessionWindow(/*sessions_count=*/5, /*selected_index=*/1));
  session_restoration_agent_->RestoreSessionWindow(window);

  ASSERT_EQ(5, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(1),
            web_state_list_->GetActiveWebState());
}

// Tests that restoring a session works correctly on non empty WebStatelist.
TEST_F(SessionRestorationBrowserAgentTest,
       RestoreSessionWithNonEmptyWebStateList) {
  web::WebState* web_state = InsertNewWebState(
      GURL(kURL1), /*parent=*/nullptr, /*index=*/0, /*background=*/false);

  SessionWindowIOS* window(
      CreateSessionWindow(/*sessions_count=*/3, /*selected_index=*/2));
  session_restoration_agent_->RestoreSessionWindow(window);

  ASSERT_EQ(4, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(3),
            web_state_list_->GetActiveWebState());
  EXPECT_EQ(web_state, web_state_list_->GetWebStateAt(0));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(1));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(2));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(3));
}

// TODO(crbug.com/888674): This test requires commiting item to
// NavigationManagerImpl which is not possible, migrate this to EG test so
// it can be tested.
TEST_F(SessionRestorationBrowserAgentTest, DISABLED_RestoreSessionOnNTPTest) {
  web::WebState* web_state =
      InsertNewWebState(GURL(kChromeUINewTabURL), /*parent=*/nullptr,
                        /*index=*/0, /*background=*/false);

  // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state);
  NewTabPageTabHelper::FromWebState(web_state)->SetDelegate(delegate);

  SessionWindowIOS* window(
      CreateSessionWindow(/*sessions_count=*/3, /*selected_index=*/2));
  session_restoration_agent_->RestoreSessionWindow(window);

  ASSERT_EQ(3, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(2),
            web_state_list_->GetActiveWebState());
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(0));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(1));
  EXPECT_NE(web_state, web_state_list_->GetWebStateAt(2));
}

// Tests that saving a non-empty session, then saving an empty session, then
// restoring, restores zero web states, and not the non-empty session.
TEST_F(SessionRestorationBrowserAgentTest, SaveAndRestoreEmptySession) {
  InsertNewWebState(GURL(kURL1), /*parent=*/nullptr, /*index=*/0,
                    /*background=*/false);

  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->SaveSession(/*immediately=*/true);
  [test_session_service_ setPerformIO:NO];

  // Session should be saved, now remove the webstate.
  web_state_list_->CloseWebStateAt(0, WebStateList::CLOSE_NO_FLAGS);
  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->SaveSession(/*immediately=*/true);
  [test_session_service_ setPerformIO:NO];

  // Restore, expect that there are no sessions.
  const base::FilePath& state_path = chrome_browser_state_->GetStatePath();
  SessionIOS* session =
      [test_session_service_ loadSessionWithSessionID:session_id()
                                            directory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];
  session_restoration_agent_->RestoreSessionWindow(session_window);

  EXPECT_EQ(0, web_state_list_->count());
}

// Tests that saving a session with web states, then clearing the WebStatelist
// and then restoring the session will restore the web states correctly.
TEST_F(SessionRestorationBrowserAgentTest, SaveAndRestoreSession) {
  web::WebState* web_state = InsertNewWebState(
      GURL(kURL1), /*parent=*/nullptr, /*index=*/0, /*background=*/false);
  InsertNewWebState(GURL(kURL1), web_state, /*index=*/1, /*background=*/false);
  InsertNewWebState(GURL(kURL2), web_state, /*index=*/0, /*background=*/false);

  ASSERT_EQ(3, web_state_list_->count());
  web_state_list_->ActivateWebStateAt(1);

  // Force state to flush to disk on the main thread so it can be immediately
  // tested below.
  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->SaveSession(/*immediately=*/true);
  [test_session_service_ setPerformIO:NO];
  // close all the webStates
  web_state_list_->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);

  const base::FilePath& state_path = chrome_browser_state_->GetStatePath();
  SessionIOS* session =
      [test_session_service_ loadSessionWithSessionID:session_id()
                                            directory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];

  // Restore from saved session.
  session_restoration_agent_->RestoreSessionWindow(session_window);

  EXPECT_EQ(3, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(1),
            web_state_list_->GetActiveWebState());
}

// Tests that SessionRestorationObserver methods are called when sessions is
// restored.
TEST_F(SessionRestorationBrowserAgentTest, ObserverCalledWithRestore) {
  InsertNewWebState(GURL(kURL1), /*parent=*/nullptr, /*index=*/0,
                    /*background=*/false);

  TestRestorationObserver observer;
  session_restoration_agent_->AddObserver(&observer);

  SessionWindowIOS* window(
      CreateSessionWindow(/*sessions_count=*/3, /*selected_index=*/2));
  session_restoration_agent_->RestoreSessionWindow(window);
  ASSERT_EQ(4, web_state_list_->count());

  EXPECT_TRUE(observer.restore_started());
  EXPECT_EQ(observer.restored_web_states_count(), 3);
  session_restoration_agent_->RemoveObserver(&observer);
}

// Tests that SessionRestorationAgent saves session when the active webState
// changes.
TEST_F(SessionRestorationBrowserAgentTest,
       SaveSessionWithActiveWebStateChange) {
  InsertNewWebState(GURL(kURL1), /*parent=*/nullptr, /*index=*/0,
                    /*background=*/true);
  InsertNewWebState(GURL(kURL2), /*parent=*/nullptr, /*index=*/1,
                    /*background=*/true);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 0);

  // Inserting new active webState.
  InsertNewWebState(GURL(kURL2), /*parent=*/nullptr, /*index=*/2,
                    /*background=*/false);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 1);

  // Removing the active webstate.
  web_state_list_->CloseWebStateAt(/*index=*/2,
                                   WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 2);

  EXPECT_EQ(web_state_list_->active_index(), 1);

  // Activating another webState without removing or inserting.
  web_state_list_->ActivateWebStateAt(/*index=*/0);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 3);

  // Removing a non active webState.
  web_state_list_->CloseWebStateAt(/*index=*/1,
                                   WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 4);

  // Removing the last active webState.
  web_state_list_->CloseWebStateAt(/*index=*/0,
                                   WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 5);
}

}  // anonymous namespace
