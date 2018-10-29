// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/features.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Defines a TabModelObserver for use in unittests.  This class can be used to
// test if an observer method was called or not.
@interface TabModelObserverPong : NSObject<TabModelObserver> {
  // TODO(crbug.com/661989): Add tests for the other observer methods.
  BOOL tabMovedWasCalled_;
}
@property(nonatomic, assign) BOOL tabMovedWasCalled;
@end

@implementation TabModelObserverPong
@synthesize tabMovedWasCalled = tabMovedWasCalled_;

- (void)tabModel:(TabModel*)model
      didMoveTab:(Tab*)tab
       fromIndex:(NSUInteger)fromIndex
         toIndex:(NSUInteger)toIndex {
  tabMovedWasCalled_ = YES;
}

@end

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
        web_client_(std::make_unique<ChromeWebClient>()) {
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
        [tab_model_ browserStateDestroyed];
        tab_model_ = nil;
      }
    }

    tab_model_ = tab_model;
    web_usage_enabler_->SetWebStateList(tab_model_.webStateList);
  }

  TabModel* CreateTabModel(SessionServiceIOS* session_service,
                           SessionWindowIOS* session_window) {
    TabModel* tab_model([[TabModel alloc]
        initWithSessionWindow:session_window
               sessionService:session_service
                 browserState:chrome_browser_state_.get()]);
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

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::ScopedTestingWebClient web_client_;
  SessionWindowIOS* session_window_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  WebStateListWebUsageEnabler* web_usage_enabler_;
  TabModel* tab_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(TabModelTest, IsEmpty) {
  EXPECT_EQ([tab_model_ count], 0U);
  EXPECT_TRUE([tab_model_ isEmpty]);
  [tab_model_ insertTabWithURL:GURL(kURL1)
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
  Tab* tab = [tab_model_ insertTabWithURL:GURL(kURL1)
                                 referrer:web::Referrer()
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:NO];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
}

TEST_P(TabModelTest, InsertUrlMultiple) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:NO];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:NO];
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:1
                              inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
}

TEST_P(TabModelTest, AppendUrlSingle) {
  Tab* tab = [tab_model_ insertTabWithURL:GURL(kURL1)
                                 referrer:web::Referrer()
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:[tab_model_ count]
                             inBackground:NO];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
}

TEST_P(TabModelTest, AppendUrlMultiple) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);
}

TEST_P(TabModelTest, CloseTabAtIndexBeginning) {
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];

  [tab_model_ closeTabAtIndex:0];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
}

TEST_P(TabModelTest, CloseTabAtIndexMiddle) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];

  [tab_model_ closeTabAtIndex:1];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
}

TEST_P(TabModelTest, CloseTabAtIndexLast) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];

  [tab_model_ closeTabAtIndex:2];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
}

TEST_P(TabModelTest, CloseTabAtIndexOnlyOne) {
  [tab_model_ insertTabWithURL:GURL(kURL1)
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

  Tab* tab = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                 referrer:web::Referrer()
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:NO];
  web::WebStateImpl* web_state = static_cast<web::WebStateImpl*>(tab.webState);
  web_state->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:1], [tab_model_ currentTab]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:0]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:2]);
}

TEST_P(TabModelTest, RestoreSessionOn2NtpTest) {
  // TODO(crbug.com/888674): migrate this to EG test so it can be tested with
  // WKBasedNavigationManager.
  if (web_client_.Get()->IsSlimNavigationManagerEnabled())
    return;

  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:NO];
  web::WebStateImpl* web_state = static_cast<web::WebStateImpl*>(tab0.webState);
  web_state->GetNavigationManagerImpl().CommitPendingItem();
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:1
                              inBackground:NO];
  web_state = static_cast<web::WebStateImpl*>(tab1.webState);
  web_state->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window];

  ASSERT_EQ(5U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:3], [tab_model_ currentTab]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:3]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:4]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:3]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:4]);
}

TEST_P(TabModelTest, RestoreSessionOnAnyTest) {
  // TODO(crbug.com/888674): migrate this to EG test so it can be tested with
  // WKBasedNavigationManager.
  if (web_client_.Get()->IsSlimNavigationManagerEnabled())
    return;

  Tab* tab = [tab_model_ insertTabWithURL:GURL(kURL1)
                                 referrer:web::Referrer()
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:NO];
  web::WebStateImpl* web_state = static_cast<web::WebStateImpl*>(tab.webState);
  web_state->GetNavigationManagerImpl().CommitPendingItem();

  SessionWindowIOS* window(CreateSessionWindow());
  [tab_model_ restoreSessionWindow:window];

  ASSERT_EQ(4U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:2], [tab_model_ currentTab]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:3]);
}

TEST_P(TabModelTest, CloseAllTabs) {
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL2)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
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

  Tab* new_tab = [tab_model_ insertTabWithURL:GURL(kURL1)
                                     referrer:web::Referrer()
                                   transition:ui::PAGE_TRANSITION_TYPED
                                       opener:nil
                                  openedByDOM:NO
                                      atIndex:[tab_model_ count]
                                 inBackground:NO];

  EXPECT_EQ([tab_model_ count], 1U);
  [tab_model_ setCurrentTab:new_tab];
  Tab* current_tab = [tab_model_ currentTab];
  EXPECT_TRUE(current_tab);
}

TEST_P(TabModelTest, AddWithOrderController) {
  // Create a few tabs with the controller at the front.
  Tab* parent = [tab_model_ insertTabWithURL:GURL(kURL1)
                                    referrer:web::Referrer()
                                  transition:ui::PAGE_TRANSITION_TYPED
                                      opener:nil
                                 openedByDOM:NO
                                     atIndex:[tab_model_ count]
                                inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];

  // Add a new tab, it should be added behind the parent.
  Tab* child =
      [tab_model_ insertTabWithURL:GURL(kURL1)
                          referrer:web::Referrer()
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 0U);
  EXPECT_EQ([tab_model_ indexOfTab:child], 1U);

  // Add another new tab without a parent, should go at the end.
  Tab* tab =
      [tab_model_ insertTabWithURL:GURL(kURL1)
                          referrer:web::Referrer()
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:nil
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab], [tab_model_ count] - 1);

  // Same for a tab that's not opened via a LINK transition.
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab2], [tab_model_ count] - 1);

  // Add a tab in the background. It should appear behind the opening tab.
  Tab* tab3 =
      [tab_model_ insertTabWithURL:GURL(kURL1)
                          referrer:web::Referrer()
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:tab
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab3], [tab_model_ indexOfTab:tab] + 1);

  // Add another background tab behind the one we just opened.
  Tab* tab4 =
      [tab_model_ insertTabWithURL:GURL(kURL1)
                          referrer:web::Referrer()
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:tab3
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab4], [tab_model_ indexOfTab:tab3] + 1);
}

TEST_P(TabModelTest, MoveTabs) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  Tab* tab2 = [tab_model_ insertTabWithURL:GURL(kURL1)
                                  referrer:web::Referrer()
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];

  // Basic sanity checks before moving on.
  ASSERT_EQ(3U, [tab_model_ count]);
  ASSERT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  ASSERT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  ASSERT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);

  // Check that observer methods are called.
  TabModelObserverPong* tab_model_observer;
  tab_model_observer = [[TabModelObserverPong alloc] init];
  [tab_model_ addObserver:tab_model_observer];

  // Move a tab from index 1 to index 0 (move tab left by one).
  [tab_model_observer setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:1] toIndex:0];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer tabMovedWasCalled]);

  // Move a tab from index 1 to index 2 (move tab right by one).
  [tab_model_observer setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:1] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer tabMovedWasCalled]);

  // Move a tab from index 0 to index 2 (move tab right by more than one).
  [tab_model_observer setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:0] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer tabMovedWasCalled]);

  // Move a tab from index 2 to index 0 (move tab left by more than one).
  [tab_model_observer setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:2] toIndex:0];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer tabMovedWasCalled]);

  // Move a tab from index 2 to index 2 (move tab to the same index).
  [tab_model_observer setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:2] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_FALSE([tab_model_observer tabMovedWasCalled]);

  // TabModel asserts that there are no observer when it is deallocated,
  // so remove the observer before the end of the method.
  [tab_model_ removeObserver:tab_model_observer];
}

TEST_P(TabModelTest, ParentTabModel) {
  std::unique_ptr<web::WebState> web_state = web::WebState::Create(
      web::WebState::CreateParams(chrome_browser_state_.get()));
  AttachTabHelpers(web_state.get(), /*for_prerender=*/false);

  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state.get());
  EXPECT_NSEQ(nil, [tab parentTabModel]);

  [tab_model_ webStateList]->InsertWebState(0, std::move(web_state),
                                            WebStateList::INSERT_FORCE_INDEX,
                                            WebStateOpener());
  EXPECT_NSEQ(tab_model_, [tab parentTabModel]);
}

TEST_P(TabModelTest, TabCreatedOnInsertion) {
  std::unique_ptr<web::WebState> web_state = web::WebState::Create(
      web::WebState::CreateParams(chrome_browser_state_.get()));

  EXPECT_NSEQ(nil, LegacyTabHelper::GetTabForWebState(web_state.get()));

  web::WebState* web_state_ptr = web_state.get();
  [tab_model_ webStateList]->InsertWebState(0, std::move(web_state),
                                            WebStateList::INSERT_FORCE_INDEX,
                                            WebStateOpener());
  EXPECT_NSNE(nil, LegacyTabHelper::GetTabForWebState(web_state_ptr));
}

TEST_P(TabModelTest, PersistSelectionChange) {
  NSString* stashPath =
      base::SysUTF8ToNSString(chrome_browser_state_->GetStatePath().value());

  // Reset the TabModel with a custom SessionServiceIOS (to control whether
  // data is saved to disk).
  TestSessionService* test_session_service = [[TestSessionService alloc] init];
  SetTabModel(CreateTabModel(test_session_service, nil));

  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:[tab_model_ tabAtIndex:0]
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:NO];
  [tab_model_ insertTabWithURL:GURL(kURL1)
                      referrer:web::Referrer()
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:[tab_model_ tabAtIndex:1]
                   openedByDOM:NO
                       atIndex:0
                  inBackground:NO];

  ASSERT_EQ(3U, [tab_model_ count]);
  [tab_model_ setCurrentTab:[tab_model_ tabAtIndex:1]];

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

  EXPECT_EQ([tab_model_ tabAtIndex:1], [tab_model_ currentTab]);

  // Clean up.
  EXPECT_TRUE([[NSFileManager defaultManager] removeItemAtPath:stashPath
                                                         error:nullptr]);
}

INSTANTIATE_TEST_CASE_P(ProgrammaticTabModelTest,
                        TabModelTest,
                        ::testing::Values(NavigationManagerChoice::LEGACY,
                                          NavigationManagerChoice::WK_BASED));

}  // anonymous namespace
