// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/keyboard/features.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/find_in_page/find_in_page_manager_impl.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class KeyCommandsProviderTest : public PlatformTest {
 protected:
  KeyCommandsProviderTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    provider_ = [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  }
  ~KeyCommandsProviderTest() override {}

  web::FakeWebState* InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    int insertedIndex = web_state_list_->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    return static_cast<web::FakeWebState*>(
        web_state_list_->GetWebStateAt(insertedIndex));
  }

  void CloseWebState(int index) {
    web_state_list_->CloseWebStateAt(
        0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }

  // Checks that `view_controller_` can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return [provider_ canPerformAction:NSSelectorFromString(action)
                            withSender:sender];
  }

  // Checks that `view_controller_` can perform the `action`. The sender is set
  // to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  // Creates a web state with a back list with 2 elements.
  web::FakeWebState* InsertNewWebPageWithMultipleEntries(int index) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();

    std::unique_ptr<web::FakeNavigationManager> navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    GURL url1("http:/test1.test/");
    navigation_manager->AddItem(url1, ui::PageTransition::PAGE_TRANSITION_LINK);
    GURL url2("http:/test2.test/");
    navigation_manager->AddItem(url2, ui::PageTransition::PAGE_TRANSITION_LINK);
    GURL url3("http:/test3.test/");
    navigation_manager->AddItem(url3, ui::PageTransition::PAGE_TRANSITION_LINK);

    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());

    int insertedIndex = web_state_list_->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    return static_cast<web::FakeWebState*>(
        web_state_list_->GetWebStateAt(insertedIndex));
  }

  void ExpectUMA(NSString* action, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [provider_ performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
  SceneState* scene_state_;
  base::UserActionTester user_action_tester_;
  KeyCommandsProvider* provider_;
};

// Checks that the nextResponder is nil by default.
TEST_F(KeyCommandsProviderTest, NextResponderUnset) {
  EXPECT_EQ(provider_.nextResponder, nil);
}

// Checks that the nextResponder is correctly set.
TEST_F(KeyCommandsProviderTest, NextResponderSet) {
  UIResponder* responder = [[UIResponder alloc] init];

  [provider_ respondBetweenViewController:nil andResponder:responder];

  EXPECT_EQ(provider_.nextResponder, responder);
}

// Checks that nextResponder is reset to nil.
TEST_F(KeyCommandsProviderTest, NextResponderReset) {
  UIResponder* responder = [[UIResponder alloc] init];
  [provider_ respondBetweenViewController:nil andResponder:responder];
  ASSERT_EQ(provider_.nextResponder, responder);

  [provider_ respondBetweenViewController:nil andResponder:nil];

  EXPECT_EQ(provider_.nextResponder, nil);
}

// Checks that KeyCommandsProvider returns key commands when the Keyboard
// Shortcuts Menu feature is enabled.
TEST_F(KeyCommandsProviderTest, ReturnsKeyCommands_MenuEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kKeyboardShortcutsMenu},
      /*disabled_features=*/{});

  EXPECT_NE(0u, provider_.keyCommands.count);
}

// Checks that KeyCommandsProvider returns key commands when the Keyboard
// Shortcuts Menu feature is disabled.
TEST_F(KeyCommandsProviderTest, ReturnsKeyCommands_MenuDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kKeyboardShortcutsMenu});

  EXPECT_NE(0u, provider_.keyCommands.count);
}

// Checks whether KeyCommandsProvider can perform the actions that are always
// available.
TEST_F(KeyCommandsProviderTest, CanPerform_AlwaysAvailableActions) {
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewRegularTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewIncognitoTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewWindow"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewIncognitoWindow"));
  EXPECT_TRUE(CanPerform(@"keyCommand_reopenLastClosedTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_showSettings"));
  EXPECT_TRUE(CanPerform(@"keyCommand_reportAnIssue"));
  EXPECT_TRUE(CanPerform(@"keyCommand_showReadingList"));
  EXPECT_TRUE(CanPerform(@"keyCommand_goToTabGrid"));
  EXPECT_TRUE(CanPerform(@"keyCommand_clearBrowsingData"));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs.
TEST_F(KeyCommandsProviderTest, CanPerform_TabsActions) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  NSArray<NSString*>* actions = @[
    @"keyCommand_openLocation",
    @"keyCommand_closeTab",
    @"keyCommand_showBookmarks",
    @"keyCommand_reload",
    @"keyCommand_showHistory",
    @"keyCommand_voiceSearch",
    @"keyCommand_stop",
    @"keyCommand_showHelp",
    @"keyCommand_showDownloads",
    @"keyCommand_showFirstTab",
    @"keyCommand_showTab2",
    @"keyCommand_showTab3",
    @"keyCommand_showTab4",
    @"keyCommand_showTab5",
    @"keyCommand_showTab6",
    @"keyCommand_showTab7",
    @"keyCommand_showTab8",
    @"keyCommand_showLastTab",
  ];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a tab.
  InsertNewWebState(0);
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  // Close the tab.
  CloseWebState(0);
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and Find in Page is available. Ensure that Find
// Next and Find Previous are not shown.
TEST_F(KeyCommandsProviderTest, CanPerform_FindInPageActions) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Open a tab.
  web::FakeWebState* web_state = InsertNewWebState(0);
  web::FindInPageManagerImpl::CreateForWebState(web_state);
  FindTabHelper::CreateForWebState(web_state);

  // No Find in Page.
  web_state->SetContentIsHTML(false);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));

  // Can Find in Page.
  web_state->SetContentIsHTML(true);
  EXPECT_TRUE(CanPerform(@"keyCommand_find"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Find UI active.
  FindTabHelper* helper = FindTabHelper::FromWebState(web_state);
  helper->SetFindUIActive(YES);
  EXPECT_TRUE(CanPerform(@"keyCommand_findNext"));
  EXPECT_TRUE(CanPerform(@"keyCommand_findPrevious"));

  helper->SetFindUIActive(NO);
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Close the tab.
  CloseWebState(0);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and text is being edited.
TEST_F(KeyCommandsProviderTest, CanPerform_EditingTextActions) {
  // back_2 and forward_2 conflict with text editing commands, so they should be
  // ignored.
  UIKeyCommand* back_2 = UIKeyCommand.cr_back_2;
  UIKeyCommand* forward_2 = UIKeyCommand.cr_forward_2;
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);

  EXPECT_FALSE(CanPerform(@"keyCommand_back"));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));

  // Add one with back and forward list not empty.
  web::FakeWebState* web_state = InsertNewWebPageWithMultipleEntries(0);
  // Ensure you have go back and go forward enabled.
  web_navigation_util::GoBack(web_state);

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_TRUE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward", forward_2));

  // Focus a text field.
  UITextField* textField = [[UITextField alloc] init];
  [GetAnyKeyWindow() addSubview:textField];
  [textField becomeFirstResponder];

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));

  // Reset the first responder.
  [textField resignFirstResponder];

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_TRUE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward", forward_2));

  // Close the tab.
  CloseWebState(0);

  EXPECT_FALSE(CanPerform(@"keyCommand_back"));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when canDismissModals is ON.
TEST_F(KeyCommandsProviderTest, CanPerform_CanDismissModalsActions) {
  ASSERT_FALSE(provider_.canDismissModals);
  EXPECT_FALSE(CanPerform(@"keyCommand_close"));

  provider_.canDismissModals = YES;
  EXPECT_TRUE(CanPerform(@"keyCommand_close"));
}

// Checks that KeyCommandsProvider implements the following actions.
TEST_F(KeyCommandsProviderTest, ImplementsActions) {
  [provider_ keyCommand_openNewTab];
  [provider_ keyCommand_openNewRegularTab];
  [provider_ keyCommand_openNewIncognitoTab];
  [provider_ keyCommand_openNewWindow];
  [provider_ keyCommand_openNewIncognitoWindow];
  [provider_ keyCommand_reopenLastClosedTab];
  [provider_ keyCommand_find];
  [provider_ keyCommand_findNext];
  [provider_ keyCommand_findPrevious];
  [provider_ keyCommand_openLocation];
  [provider_ keyCommand_closeTab];
  [provider_ keyCommand_showNextTab];
  [provider_ keyCommand_showPreviousTab];
  [provider_ keyCommand_showBookmarks];
  [provider_ keyCommand_addToBookmarks];
  [provider_ keyCommand_reload];
  [provider_ keyCommand_back];
  [provider_ keyCommand_forward];
  [provider_ keyCommand_showHistory];
  [provider_ keyCommand_voiceSearch];
  [provider_ keyCommand_close];
  [provider_ keyCommand_showSettings];
  [provider_ keyCommand_stop];
  [provider_ keyCommand_showHelp];
  [provider_ keyCommand_showDownloads];
  [provider_ keyCommand_showFirstTab];
  [provider_ keyCommand_showTab2];
  [provider_ keyCommand_showTab3];
  [provider_ keyCommand_showTab4];
  [provider_ keyCommand_showTab5];
  [provider_ keyCommand_showTab6];
  [provider_ keyCommand_showTab7];
  [provider_ keyCommand_showTab8];
  [provider_ keyCommand_showLastTab];
  [provider_ keyCommand_reportAnIssue];
  [provider_ keyCommand_addToReadingList];
  [provider_ keyCommand_showReadingList];
  [provider_ keyCommand_goToTabGrid];
  [provider_ keyCommand_clearBrowsingData];
}

// Checks that metrics are correctly reported.
TEST_F(KeyCommandsProviderTest, Metrics) {
  ExpectUMA(@"keyCommand_openNewTab", "MobileKeyCommandOpenNewTab");
  ExpectUMA(@"keyCommand_openNewRegularTab",
            "MobileKeyCommandOpenNewRegularTab");
  ExpectUMA(@"keyCommand_openNewIncognitoTab",
            "MobileKeyCommandOpenNewIncognitoTab");
  ExpectUMA(@"keyCommand_openNewWindow", "MobileKeyCommandOpenNewWindow");
  ExpectUMA(@"keyCommand_openNewIncognitoWindow",
            "MobileKeyCommandOpenNewIncognitoWindow");
  ExpectUMA(@"keyCommand_reopenLastClosedTab",
            "MobileKeyCommandReopenLastClosedTab");
  ExpectUMA(@"keyCommand_find", "MobileKeyCommandFind");
  ExpectUMA(@"keyCommand_findNext", "MobileKeyCommandFindNext");
  ExpectUMA(@"keyCommand_findPrevious", "MobileKeyCommandFindPrevious");
  ExpectUMA(@"keyCommand_openLocation", "MobileKeyCommandOpenLocation");
  ExpectUMA(@"keyCommand_closeTab", "MobileKeyCommandCloseTab");
  ExpectUMA(@"keyCommand_showNextTab", "MobileKeyCommandShowNextTab");
  ExpectUMA(@"keyCommand_showPreviousTab", "MobileKeyCommandShowPreviousTab");
  ExpectUMA(@"keyCommand_showBookmarks", "MobileKeyCommandShowBookmarks");
  ExpectUMA(@"keyCommand_addToBookmarks", "MobileKeyCommandAddToBookmarks");
  ExpectUMA(@"keyCommand_reload", "MobileKeyCommandReload");
  ExpectUMA(@"keyCommand_back", "MobileKeyCommandBack");
  ExpectUMA(@"keyCommand_forward", "MobileKeyCommandForward");
  ExpectUMA(@"keyCommand_showHistory", "MobileKeyCommandShowHistory");
  ExpectUMA(@"keyCommand_voiceSearch", "MobileKeyCommandVoiceSearch");
  ExpectUMA(@"keyCommand_close", "MobileKeyCommandClose");
  ExpectUMA(@"keyCommand_showSettings", "MobileKeyCommandShowSettings");
  ExpectUMA(@"keyCommand_stop", "MobileKeyCommandStop");
  ExpectUMA(@"keyCommand_showHelp", "MobileKeyCommandShowHelp");
  ExpectUMA(@"keyCommand_showDownloads", "MobileKeyCommandShowDownloads");
  ExpectUMA(@"keyCommand_showFirstTab", "MobileKeyCommandShowFirstTab");
  ExpectUMA(@"keyCommand_showTab2", "MobileKeyCommandShowTab2");
  ExpectUMA(@"keyCommand_showTab3", "MobileKeyCommandShowTab3");
  ExpectUMA(@"keyCommand_showTab4", "MobileKeyCommandShowTab4");
  ExpectUMA(@"keyCommand_showTab5", "MobileKeyCommandShowTab5");
  ExpectUMA(@"keyCommand_showTab6", "MobileKeyCommandShowTab6");
  ExpectUMA(@"keyCommand_showTab7", "MobileKeyCommandShowTab7");
  ExpectUMA(@"keyCommand_showTab8", "MobileKeyCommandShowTab8");
  ExpectUMA(@"keyCommand_showLastTab", "MobileKeyCommandShowLastTab");
  ExpectUMA(@"keyCommand_reportAnIssue", "MobileKeyCommandReportAnIssue");
  ExpectUMA(@"keyCommand_addToReadingList", "MobileKeyCommandAddToReadingList");
  ExpectUMA(@"keyCommand_showReadingList", "MobileKeyCommandShowReadingList");
  ExpectUMA(@"keyCommand_goToTabGrid", "MobileKeyCommandGoToTabGrid");
  ExpectUMA(@"keyCommand_clearBrowsingData",
            "MobileKeyCommandClearBrowsingData");
}

// Checks the next/previous tab actions work OK.
TEST_F(KeyCommandsProviderTest, NextPreviousTab) {
  InsertNewWebState(0);
  InsertNewWebState(1);
  InsertNewWebState(2);
  ASSERT_EQ(web_state_list_->count(), 3);

  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 1);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 2);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 2);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 1);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
}

// Verifies that nothing is added to Reading List when there is no tab.
TEST_F(KeyCommandsProviderTest, AddToReadingList_DoesntAddWhenNoTab) {
  provider_.dispatcher = OCMStrictProtocolMock(@protocol(ApplicationCommands));

  [provider_ keyCommand_addToReadingList];
}

// Verifies that nothing is added to Reading List when on the NTP.
TEST_F(KeyCommandsProviderTest, AddToReadingList_DoesntAddWhenNTP) {
  provider_.dispatcher = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  InsertNewWebState(0);

  [provider_ keyCommand_addToReadingList];
}

// Verifies that the correct URL is added to Reading List.
TEST_F(KeyCommandsProviderTest, AddToReadingList_AddURL) {
  id handler = OCMStrictProtocolMock(@protocol(BrowserCommands));
  provider_.dispatcher = handler;
  GURL url = GURL("https://e.test");
  id addCommand = [OCMArg checkWithBlock:^BOOL(ReadingListAddCommand* command) {
    return command.URLs.count == 1 && command.URLs.firstObject.URL == url;
  }];
  OCMExpect([provider_.dispatcher addToReadingList:addCommand]);
  web::FakeWebState* web_state = InsertNewWebState(0);
  web_state->SetCurrentURL(url);

  [provider_ keyCommand_addToReadingList];

  [handler verify];
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are at least two tabs.
TEST_F(KeyCommandsProviderTest, CanPerform_ShowPreviousAndNextTab) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  NSArray<NSString*>* actions = @[
    @"keyCommand_showNextTab",
    @"keyCommand_showPreviousTab",
  ];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a tab.
  InsertNewWebState(0);
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a second tab.
  InsertNewWebState(1);
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  // Close the one tab.
  CloseWebState(0);
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and it is a http or https page.
TEST_F(KeyCommandsProviderTest, CanPerform_ActionsInHttpPage) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  NSArray<NSString*>* actions =
      @[ @"keyCommand_addToBookmarks", @"keyCommand_addToReadingList" ];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a New Tab Page (NTP) tab which is not a http or https page.
  std::unique_ptr<web::FakeNavigationManager> fake_navigation_manager =
      std::make_unique<web::FakeNavigationManager>();

  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  pending_item->SetURL(GURL(kChromeUIAboutNewTabURL));

  fake_navigation_manager->SetPendingItem(pending_item.get());
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>();

  GURL url(kChromeUINewTabURL);
  fake_web_state->SetVisibleURL(url);
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
  fake_web_state->SetBrowserState(browser_state_.get());

  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));

  NewTabPageTabHelper::CreateForWebState(fake_web_state.get());
  NewTabPageTabHelper* ntp_helper =
      NewTabPageTabHelper::FromWebState(fake_web_state.get());
  ntp_helper->SetDelegate(delegate);

  // Ensure that the actions are not available when the tab is a NTP.
  ASSERT_TRUE(ntp_helper->IsActive());
  ASSERT_FALSE(url.SchemeIsHTTPOrHTTPS());
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a second tab which is a http one.
  web::FakeWebState* second_tab_web_state = InsertNewWebState(1);
  GURL http_url("http://foo/");
  second_tab_web_state->SetVisibleURL(http_url);

  // Ensure that the actions are available.
  ASSERT_TRUE(http_url.SchemeIsHTTPOrHTTPS());
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are back or forward navigations.
TEST_F(KeyCommandsProviderTest, CanPerform_BackForwardWithMultipleEntries) {
  web::FakeWebState* web_state = InsertNewWebPageWithMultipleEntries(0);

  NSString* goBackActions = @"keyCommand_back";
  NSString* goForwardActions = @"keyCommand_forward";

  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_FALSE(CanPerform(goForwardActions));

  web_navigation_util::GoBack(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoBack(web_state);
  EXPECT_FALSE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoForward(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoForward(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_FALSE(CanPerform(goForwardActions));
}

}  // namespace
