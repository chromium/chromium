// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#import "ios/chrome/test/fakes/fake_web_state_list_observing_delegate.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"
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

// TabModelTest is parameterized on this enum to test both
// LegacyNavigationManager and WKBasedNavigationManager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

class TabModelTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 public:
  TabModelTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        web_client_(std::make_unique<ChromeWebClient>()),
        web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    // Web usage is disabled during these tests.
    web_usage_enabler_ =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get());
    web_usage_enabler_->SetWebUsageEnabled(false);

    session_window_ = [[SessionWindowIOS alloc] init];

    // Create tab model with just a dummy session service so the async state
    // saving doesn't trigger unless actually wanted.
    SetTabModel(CreateTabModel([[TestSessionService alloc] init], nil));
  }

  ~TabModelTest() override = default;

  void TearDown() override {
    SetTabModel(nil);
    PlatformTest::TearDown();
  }

  void SetTabModel(TabModel* tab_model) {
    if (tab_model_) {
      web_usage_enabler_->SetWebStateList(nullptr);
      @autoreleasepool {
        [tab_model_ disconnect];
        tab_model_ = nil;
      }
    }

    tab_model_ = tab_model;
    web_usage_enabler_->SetWebStateList(tab_model_.webStateList);
  }

  TabModel* CreateTabModel(SessionServiceIOS* session_service,
                           SessionWindowIOS* session_window) {
    TabModel* tab_model([[TabModel alloc]
        initWithSessionService:session_service
                  browserState:chrome_browser_state_.get()
                  webStateList:web_state_list_.get()]);
    [tab_model restoreSessionWindow:session_window forInitialRestore:YES];
    [tab_model setPrimary:YES];
    return tab_model;
  }

 protected:
  // Creates a session window with entries named "restored window 1",
  // "restored window 2" and "restored window 3" and the second entry
  // marked as selected.
  SessionWindowIOS* CreateSessionWindow() {
    NSMutableArray<CRWSessionStorage*>* sessions = [NSMutableArray array];
    for (int i = 0; i < 3; i++) {
      CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
      session_storage.lastCommittedItemIndex = -1;
      [sessions addObject:session_storage];
    }
    return [[SessionWindowIOS alloc] initWithSessions:sessions selectedIndex:1];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  SessionWindowIOS* session_window_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  WebStateListWebUsageEnabler* web_usage_enabler_;
  TabModel* tab_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(TabModelTest, IsEmpty) {
  EXPECT_EQ([tab_model_ count], 0U);
  EXPECT_TRUE([tab_model_ isEmpty]);
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:0
                       inBackground:NO];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_FALSE([tab_model_ isEmpty]);
}

TEST_P(TabModelTest, InsertUrlSingle) {
  web::WebState* web_state =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_EQ(web_state, tab_model_.webStateList->GetWebStateAt(0));
}

TEST_P(TabModelTest, BrowserStateDestroyedMultiple) {
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:0
                       inBackground:NO];
  [tab_model_ disconnect];
  [tab_model_ disconnect];
}

TEST_P(TabModelTest, InsertUrlMultiple) {
  web::WebState* web_state0 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  web::WebState* web_state1 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  web::WebState* web_state2 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:1
                           inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_EQ(web_state1, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state2, tab_model_.webStateList->GetWebStateAt(1));
  EXPECT_EQ(web_state0, tab_model_.webStateList->GetWebStateAt(2));
}

TEST_P(TabModelTest, AppendUrlSingle) {
  web::WebState* web_state =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_EQ(web_state, tab_model_.webStateList->GetWebStateAt(0));
}

TEST_P(TabModelTest, AppendUrlMultiple) {
  web::WebState* web_state0 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  web::WebState* web_state1 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  web::WebState* web_state2 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_EQ(web_state0, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state1, tab_model_.webStateList->GetWebStateAt(1));
  EXPECT_EQ(web_state2, tab_model_.webStateList->GetWebStateAt(2));
}

TEST_P(TabModelTest, CloseTabAtIndexBeginning) {
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  web::WebState* web_state1 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  web::WebState* web_state2 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];

  [tab_model_ closeTabAtIndex:0];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_EQ(web_state1, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state2, tab_model_.webStateList->GetWebStateAt(1));
}

TEST_P(TabModelTest, CloseTabAtIndexMiddle) {
  web::WebState* web_state0 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  web::WebState* web_state2 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];

  [tab_model_ closeTabAtIndex:1];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_EQ(web_state0, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state2, tab_model_.webStateList->GetWebStateAt(1));
}

TEST_P(TabModelTest, CloseTabAtIndexLast) {
  web::WebState* web_state0 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  web::WebState* web_state1 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];

  [tab_model_ closeTabAtIndex:2];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_EQ(web_state0, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state1, tab_model_.webStateList->GetWebStateAt(1));
}

TEST_P(TabModelTest, CloseTabAtIndexOnlyOne) {
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];

  [tab_model_ closeTabAtIndex:0];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_P(TabModelTest, RestoreSessionOnNTPTest) {
  // TODO(crbug.com/888674): migrate this to EG test so it can be tested with
  // WKBasedNavigationManager.
  if (web_client_.Get()->IsSlimNavigationManagerEnabled())
    return;

  web::WebState* web_state =
      [tab_model_ insertWebStateWithURL:GURL(kChromeUINewTabURL)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);

  // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state, delegate);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_EQ(tab_model_.webStateList->GetWebStateAt(1),
            tab_model_.webStateList->GetActiveWebState());
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(1));
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(2));
}

TEST_P(TabModelTest, RestoreSessionOn2NtpTest) {
  // TODO(crbug.com/888674): migrate this to EG test so it can be tested with
  // WKBasedNavigationManager.
  if (web_client_.Get()->IsSlimNavigationManagerEnabled())
    return;

  web::WebState* web_state0 =
      [tab_model_ insertWebStateWithURL:GURL(kChromeUINewTabURL)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state0);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();
  web::WebState* web_state1 =
      [tab_model_ insertWebStateWithURL:GURL(kChromeUINewTabURL)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:1
                           inBackground:NO];
  web_state_impl = static_cast<web::WebStateImpl*>(web_state1);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(5U, [tab_model_ count]);
  EXPECT_EQ(tab_model_.webStateList->GetWebStateAt(3),
            tab_model_.webStateList->GetActiveWebState());
  EXPECT_EQ(web_state0, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_EQ(web_state1, tab_model_.webStateList->GetWebStateAt(1));
  EXPECT_NE(web_state0, tab_model_.webStateList->GetWebStateAt(2));
  EXPECT_NE(web_state0, tab_model_.webStateList->GetWebStateAt(3));
  EXPECT_NE(web_state0, tab_model_.webStateList->GetWebStateAt(4));
  EXPECT_NE(web_state1, tab_model_.webStateList->GetWebStateAt(2));
  EXPECT_NE(web_state1, tab_model_.webStateList->GetWebStateAt(3));
  EXPECT_NE(web_state1, tab_model_.webStateList->GetWebStateAt(4));
}

TEST_P(TabModelTest, RestoreSessionOnAnyTest) {
  // TODO(crbug.com/888674): migrate this to EG test so it can be tested with
  // WKBasedNavigationManager.
  if (web_client_.Get()->IsSlimNavigationManagerEnabled())
    return;

  web::WebState* web_state =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:0
                           inBackground:NO];
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);
  web_state_impl->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window forInitialRestore:NO];

  ASSERT_EQ(4U, [tab_model_ count]);
  EXPECT_EQ(tab_model_.webStateList->GetWebStateAt(2),
            tab_model_.webStateList->GetActiveWebState());
  EXPECT_EQ(web_state, tab_model_.webStateList->GetWebStateAt(0));
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(1));
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(2));
  EXPECT_NE(web_state, tab_model_.webStateList->GetWebStateAt(3));
}

TEST_P(TabModelTest, CloseAllTabs) {
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL2)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];

  [tab_model_ closeAllTabs];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_P(TabModelTest, CloseAllTabsWithNoTabs) {
  [tab_model_ closeAllTabs];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_P(TabModelTest, InsertWithSessionController) {
  EXPECT_EQ([tab_model_ count], 0U);
  EXPECT_TRUE([tab_model_ isEmpty]);

  web::WebState* new_web_state =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];

  EXPECT_EQ([tab_model_ count], 1U);
  EXPECT_EQ(new_web_state, tab_model_.webStateList->GetWebStateAt(0));
  tab_model_.webStateList->ActivateWebStateAt(0);
  web::WebState* current_web_state =
      tab_model_.webStateList->GetActiveWebState();
  EXPECT_TRUE(current_web_state);
}

TEST_P(TabModelTest, AddWithOrderController) {
  // Create a few tabs with the controller at the front.
  web::WebState* parent =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];

  // Add a new tab, it should be added behind the parent.
  web::WebState* child = [tab_model_
      insertWebStateWithURL:GURL(kURL1)
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_LINK
                     opener:parent
                openedByDOM:NO
                    atIndex:TabModelConstants::kTabPositionAutomatically
               inBackground:NO];
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(parent), 0);
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(child), 1);

  // Add another new tab without a parent, should go at the end.
  web::WebState* web_state = [tab_model_
      insertWebStateWithURL:GURL(kURL1)
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_LINK
                     opener:nil
                openedByDOM:NO
                    atIndex:TabModelConstants::kTabPositionAutomatically
               inBackground:NO];
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(web_state),
            static_cast<int>([tab_model_ count]) - 1);

  // Same for a tab that's not opened via a LINK transition.
  web::WebState* web_state2 =
      [tab_model_ insertWebStateWithURL:GURL(kURL1)
                               referrer:web::Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                                 opener:nil
                            openedByDOM:NO
                                atIndex:[tab_model_ count]
                           inBackground:NO];
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(web_state2),
            static_cast<int>([tab_model_ count]) - 1);

  // Add a tab in the background. It should appear behind the opening tab.
  web::WebState* web_state3 = [tab_model_
      insertWebStateWithURL:GURL(kURL1)
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_LINK
                     opener:web_state
                openedByDOM:NO
                    atIndex:TabModelConstants::kTabPositionAutomatically
               inBackground:NO];
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(web_state3),
            tab_model_.webStateList->GetIndexOfWebState(web_state) + 1);

  // Add another background tab behind the one we just opened.
  web::WebState* web_state4 = [tab_model_
      insertWebStateWithURL:GURL(kURL1)
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_LINK
                     opener:web_state3
                openedByDOM:NO
                    atIndex:TabModelConstants::kTabPositionAutomatically
               inBackground:NO];
  EXPECT_EQ(tab_model_.webStateList->GetIndexOfWebState(web_state4),
            tab_model_.webStateList->GetIndexOfWebState(web_state3) + 1);
}

// Test that saving a non-empty session, then saving an empty session, then
// restoring, restores zero tabs, and not the non-empty session.
TEST_P(TabModelTest, RestorePersistedSessionAfterEmpty) {
  // Reset the TabModel with a custom SessionServiceIOS (to control whether
  // data is saved to disk).
  TestSessionService* test_session_service = [[TestSessionService alloc] init];
  SetTabModel(CreateTabModel(test_session_service, nil));

  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:0
                       inBackground:NO];
  [test_session_service setPerformIO:YES];
  [tab_model_ saveSessionImmediately:YES];
  [test_session_service setPerformIO:NO];

  // Session should be saved, now remove the tab.
  [tab_model_ closeTabAtIndex:0];
  [test_session_service setPerformIO:YES];
  [tab_model_ saveSessionImmediately:YES];
  [test_session_service setPerformIO:NO];

  // Restore, expect that there are no sessions.
  NSString* state_path = base::SysUTF8ToNSString(
      chrome_browser_state_->GetStatePath().AsUTF8Unsafe());
  SessionIOS* session =
      [test_session_service loadSessionFromDirectory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];
  [tab_model_ restoreSessionWindow:session_window forInitialRestore:NO];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_P(TabModelTest, DISABLED_PersistSelectionChange) {
  // Reset the TabModel with a custom SessionServiceIOS (to control whether
  // data is saved to disk).
  TestSessionService* test_session_service = [[TestSessionService alloc] init];
  SetTabModel(CreateTabModel(test_session_service, nil));

  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:nil
                        openedByDOM:NO
                            atIndex:tab_model_.webStateList->count()
                       inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:tab_model_.webStateList->GetWebStateAt(0)
                        openedByDOM:NO
                            atIndex:[tab_model_ count]
                       inBackground:NO];
  [tab_model_ insertWebStateWithURL:GURL(kURL1)
                           referrer:web::Referrer()
                         transition:ui::PAGE_TRANSITION_TYPED
                             opener:tab_model_.webStateList->GetWebStateAt(0)
                        openedByDOM:NO
                            atIndex:0
                       inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  tab_model_.webStateList->ActivateWebStateAt(1);
  // Force state to flush to disk on the main thread so it can be immediately
  // tested below.
  [test_session_service setPerformIO:YES];
  [tab_model_ saveSessionImmediately:YES];
  [test_session_service setPerformIO:NO];

  NSString* state_path = base::SysUTF8ToNSString(
      chrome_browser_state_->GetStatePath().AsUTF8Unsafe());
  SessionIOS* session =
      [test_session_service loadSessionFromDirectory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];

  // Create tab model from saved session.
  SetTabModel(CreateTabModel(test_session_service, session_window));

  ASSERT_EQ(3u, [tab_model_ count]);

  EXPECT_EQ(tab_model_.webStateList->GetWebStateAt(1),
            tab_model_.webStateList->GetActiveWebState());
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticTabModelTest,
                         TabModelTest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));

}  // anonymous namespace
