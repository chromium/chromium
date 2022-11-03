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
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/keyboard/features.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/find_in_page/find_in_page_manager_impl.h"
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

#pragma mark - KeyCommandsProvider Tests with Keyboard Shortcuts Menu enabled

class KeyCommandsProviderTest : public PlatformTest {
 protected:
  KeyCommandsProviderTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kKeyboardShortcutsMenu},
        /*disabled_features=*/{});
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

  void ExpectUMA(NSString* selector, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [provider_ performSelector:NSSelectorFromString(selector)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
  SceneState* scene_state_;
  base::test::ScopedFeatureList feature_list_;
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

// Checks that KeyCommandsProvider returns key commands.
TEST_F(KeyCommandsProviderTest, ReturnsKeyCommands) {
  EXPECT_NE(0u, provider_.keyCommands.count);
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
  ;
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

#pragma mark - KeyCommandsProvider Tests with Keyboard Shortcuts Menu disabled

class NoMenuKeyCommandsProviderTest : public PlatformTest {
 protected:
  NoMenuKeyCommandsProviderTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kKeyboardShortcutsMenu});
  }
  ~NoMenuKeyCommandsProviderTest() override {}

  web::FakeWebState* InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    int insertedIndex = web_state_list_->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    return static_cast<web::FakeWebState*>(
        web_state_list_->GetWebStateAt(insertedIndex));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NoMenuKeyCommandsProviderTest, NoTabs_ReturnsObjects) {
  id<ApplicationCommands, BrowserCommands, BrowserCoordinatorCommands,
     FindInPageCommands>
      dispatcher = nil;

  KeyCommandsProvider* provider =
      [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  provider.dispatcher = dispatcher;

  // No tabs.
  EXPECT_EQ(web_state_list_->count(), 0);

  EXPECT_NE(0u, provider.keyCommands.count);
}

TEST_F(NoMenuKeyCommandsProviderTest, ReturnsKeyCommandsObjects) {
  id<ApplicationCommands, BrowserCommands, BrowserCoordinatorCommands,
     FindInPageCommands>
      dispatcher = nil;

  KeyCommandsProvider* provider =
      [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  provider.dispatcher = dispatcher;

  // No tabs.
  EXPECT_EQ(web_state_list_->count(), 0);

  for (id element in provider.keyCommands) {
    EXPECT_TRUE([element isKindOfClass:[UIKeyCommand class]]);
  }
}

TEST_F(NoMenuKeyCommandsProviderTest, MoreKeyboardCommandsWhenTabs) {
  id<ApplicationCommands, BrowserCommands, BrowserCoordinatorCommands,
     FindInPageCommands>
      dispatcher = nil;

  KeyCommandsProvider* provider =
      [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  provider.dispatcher = dispatcher;

  // No tabs.
  EXPECT_EQ(web_state_list_->count(), 0);

  NSUInteger numberOfKeyCommandsWithoutTabs = provider.keyCommands.count;

  InsertNewWebState(0);

  // Tabs.
  EXPECT_EQ(web_state_list_->count(), 1);
  NSUInteger numberOfKeyCommandsWithTabs = provider.keyCommands.count;

  EXPECT_GT(numberOfKeyCommandsWithTabs, numberOfKeyCommandsWithoutTabs);
}

TEST_F(NoMenuKeyCommandsProviderTest, LessKeyCommandsWhenTabsAndEditingText) {
  id<ApplicationCommands, BrowserCommands, BrowserCoordinatorCommands,
     FindInPageCommands>
      dispatcher = nil;

  KeyCommandsProvider* provider =
      [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  provider.dispatcher = dispatcher;

  // No tabs.
  EXPECT_EQ(web_state_list_->count(), 0);

  InsertNewWebState(0);

  // Tabs.
  EXPECT_EQ(web_state_list_->count(), 1);

  // Not editing text.
  NSUInteger numberOfKeyCommandsWhenNotEditingText = provider.keyCommands.count;

  // Focus a text field.
  UITextField* textField = [[UITextField alloc] init];
  [GetAnyKeyWindow() addSubview:textField];
  [textField becomeFirstResponder];

  // Editing text.
  NSUInteger numberOfKeyCommandsWhenEditingText = provider.keyCommands.count;

  EXPECT_LT(numberOfKeyCommandsWhenEditingText,
            numberOfKeyCommandsWhenNotEditingText);

  // Reset the first responder.
  [textField resignFirstResponder];
}

TEST_F(NoMenuKeyCommandsProviderTest,
       MoreKeyboardCommandsWhenFindInPageAvailable) {
  id<ApplicationCommands, BrowserCommands, BrowserCoordinatorCommands,
     FindInPageCommands>
      dispatcher = nil;

  KeyCommandsProvider* provider =
      [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  provider.dispatcher = dispatcher;

  // No tabs.
  EXPECT_EQ(web_state_list_->count(), 0);

  web::FakeWebState* web_state = InsertNewWebState(0);
  web::FindInPageManagerImpl::CreateForWebState(web_state);
  FindTabHelper::CreateForWebState(web_state);

  // Tabs.
  EXPECT_EQ(web_state_list_->count(), 1);

  // No Find in Page.
  web_state->SetContentIsHTML(false);
  NSUInteger numberOfKeyCommandsWithoutFIP = provider.keyCommands.count;

  // Can Find in Page.
  web_state->SetContentIsHTML(true);
  NSUInteger numberOfKeyCommandsWithFIP = provider.keyCommands.count;

  EXPECT_GT(numberOfKeyCommandsWithFIP, numberOfKeyCommandsWithoutFIP);
}

}  // namespace
