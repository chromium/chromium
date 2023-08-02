// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#import "base/files/file_path.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_thread.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"

namespace {

// Information about a single tab that needs to be restored.
struct TabInfo {
  int opener_index = -1;
  bool pinned = false;
};

// Information about a collection of N tabs that needs to be restored.
template <size_t N>
struct SessionInfo {
  int active_index;
  std::array<TabInfo, N> tab_infos;
};

// Creates a NSArray<CRWNavigationItemStorage*>*.
NSArray<CRWNavigationItemStorage*>* CreateNavigationStorage() {
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];
  item_storage.virtualURL = GURL("http://init.text");
  return @[ item_storage ];
}

// Create a CRWSessionUserData* from `tab_info`.
CRWSessionUserData* CreateSessionUserData(TabInfo tab_info) {
  if (!tab_info.pinned && tab_info.opener_index == -1) {
    return nil;
  }

  CRWSessionUserData* user_data = [[CRWSessionUserData alloc] init];
  if (tab_info.pinned) {
    [user_data setObject:@YES forKey:@"PinnedState"];
  }
  if (tab_info.opener_index != -1) {
    [user_data setObject:@(tab_info.opener_index) forKey:@"OpenerIndex"];
    [user_data setObject:@(0) forKey:@"OpenerNavigationIndex"];
  }
  return user_data;
}

// Creates a CRWSessionStorage* from `tab_info`.
CRWSessionStorage* CreateSessionStorage(TabInfo tab_info) {
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
  session_storage.uniqueIdentifier = SessionID::NewUnique();
  session_storage.lastCommittedItemIndex = 0;
  session_storage.itemStorages = CreateNavigationStorage();
  session_storage.userData = CreateSessionUserData(tab_info);
  return session_storage;
}

// Creates a SessionWindowIOS* from `session_info`.
template <size_t N>
SessionWindowIOS* CreateSessionWindow(SessionInfo<N> session_info) {
  if (session_info.active_index < 0) {
    return nil;
  }

  if (N <= static_cast<size_t>(session_info.active_index)) {
    return nil;
  }

  NSMutableArray<CRWSessionStorage*>* sessions =
      [[NSMutableArray alloc] initWithCapacity:N];

  for (const TabInfo& tab_info : session_info.tab_infos) {
    [sessions addObject:CreateSessionStorage(tab_info)];
  }

  return [[SessionWindowIOS alloc] initWithSessions:sessions
                                      selectedIndex:session_info.active_index];
}

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
  SessionRestorationBrowserAgentTest() {
    scoped_feature_list_.InitAndDisableFeature(
        web::features::kEnableSessionSerializationOptimizations);

    test_session_service_ = [[TestSessionService alloc] init];
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    session_identifier_ = [[NSUUID UUID] UUIDString];
  }

  ~SessionRestorationBrowserAgentTest() override = default;

  void SetUp() override {
    // This test requires that some TabHelpers are attached to the WebStates, so
    // it needs to use a WebStateList with the full BrowserWebStateListDelegate,
    // rather than the TestWebStateList delegate used in the default TestBrowser
    // constructor.
    browser_ = std::make_unique<TestBrowser>(
        chrome_browser_state_.get(),
        std::make_unique<BrowserWebStateListDelegate>());
    // Web usage is disabled during these tests.
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    web_usage_enabler_ =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    web_usage_enabler_->SetWebUsageEnabled(false);
  }

  void TearDown() override {
    @autoreleasepool {
      browser_->GetWebStateList()->CloseAllWebStates(
          WebStateList::CLOSE_NO_FLAGS);
    }
    PlatformTest::TearDown();
  }

  NSString* session_id() { return session_identifier_; }

 protected:
  void CreateSessionRestorationBrowserAgent(bool enable_pinned_web_states) {
    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), test_session_service_, enable_pinned_web_states);
    session_restoration_agent_ =
        SessionRestorationBrowserAgent::FromBrowser(browser_.get());
    session_restoration_agent_->SetSessionID(session_identifier_);
  }

  // Creates a WebState with the given parameters.
  std::unique_ptr<web::WebState> CreateWebState() {
    web::WebState::CreateParams create_params(chrome_browser_state_.get());
    create_params.created_with_opener = false;

    std::unique_ptr<web::WebState> web_state =
        web::WebState::Create(create_params);

    return web_state;
  }

  // Creates a WebState with the given parameters and insert it in the
  // Browser's WebStateList.
  web::WebState* InsertNewWebState(web::WebState* parent,
                                   int index,
                                   bool pinned,
                                   bool background) {
    std::unique_ptr<web::WebState> web_state = CreateWebState();

    int insertion_flags = WebStateList::INSERT_FORCE_INDEX;
    if (!background) {
      insertion_flags |= WebStateList::INSERT_ACTIVATE;
    }
    if (pinned) {
      insertion_flags |= WebStateList::INSERT_PINNED;
    }
    browser_->GetWebStateList()->InsertWebState(
        index, std::move(web_state), insertion_flags, WebStateOpener(parent));
    return browser_->GetWebStateList()->GetWebStateAt(index);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;

  __strong NSString* session_identifier_ = nil;
  TestSessionService* test_session_service_;
  WebUsageEnablerBrowserAgent* web_usage_enabler_;
  SessionRestorationBrowserAgent* session_restoration_agent_;
};

// Tests that CRWSessionStorage with empty item_storages are not restored.
TEST_F(SessionRestorationBrowserAgentTest, RestoreEmptySessions) {
  CreateSessionRestorationBrowserAgent(true);

  NSMutableArray<CRWSessionStorage*>* sessions = [NSMutableArray array];
  for (int i = 0; i < 3; i++) {
    CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
    session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
    session_storage.uniqueIdentifier = SessionID::NewUnique();
    session_storage.lastCommittedItemIndex = -1;
    [sessions addObject:session_storage];
  }
  SessionWindowIOS* window = [[SessionWindowIOS alloc] initWithSessions:sessions
                                                          selectedIndex:2];

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);
  ASSERT_EQ(0, browser_->GetWebStateList()->count());
}

// Tests that restoring a session works correctly on empty WebStateList.
TEST_F(SessionRestorationBrowserAgentTest, RestoreSessionOnEmptyWebStateList) {
  CreateSessionRestorationBrowserAgent(true);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 1,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.opener_index = 0},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});
  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);

  ASSERT_EQ(5, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(1),
            browser_->GetWebStateList()->GetActiveWebState());

  // Check that the opener has correctly be restored.
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(0),
            browser_->GetWebStateList()->GetOpenerOfWebStateAt(1).opener);

  // Check that the first tab is pinned.
  ASSERT_TRUE(browser_->GetWebStateList()->IsWebStatePinnedAt(0));
}

// Tests that restoring a session works correctly on non empty WebStatelist.
TEST_F(SessionRestorationBrowserAgentTest,
       RestoreSessionWithNonEmptyWebStateList) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* web_state =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<3>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});
  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);

  ASSERT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(3),
            browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_EQ(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(3));
}

// Tests that restoring a session with scope `kAll` works correctly on non
// empty WebStatelist with pinned WebStates present.
TEST_F(SessionRestorationBrowserAgentTest, RestoreAllWebStatesInSession) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 1,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);

  ASSERT_EQ(12, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(4),
            browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(6));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(7));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(8));
}

// Tests that restoring a session with scope `kPinnedOnly` works correctly on
// non empty WebStatelist with pinned WebStates present.
TEST_F(SessionRestorationBrowserAgentTest,
       RestorePinnedWebStatesOnlyInSession) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kPinnedOnly);

  ASSERT_EQ(9, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            regular_web_state_3);
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(6));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(7));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(8));
}

// Tests that restoring a session with scope `kRegularOnly` works correctly on
// non empty WebStatelist with pinned WebStates present.
TEST_F(SessionRestorationBrowserAgentTest,
       RestoreRegularWebStatesOnlyInSession) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 3,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kRegularOnly);

  ASSERT_EQ(10, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            browser_->GetWebStateList()->GetWebStateAt(8));
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(3));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(4));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(6));
}

// Tests that restoring a session with scope `kAll` but disabled pinned tabs
// works correctly on non empty WebStatelist with pinned WebStates present.
TEST_F(SessionRestorationBrowserAgentTest,
       RestoreAllWebStatesInSessionWithPinnedTabsDisabled) {
  CreateSessionRestorationBrowserAgent(false);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);

  ASSERT_EQ(12, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(9),
            browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(3));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(4));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(6));
}

// Tests that restoring a session with scope `kPinnedOnly` but disabled pinned
// tabs works correctly on non empty WebStatelist with pinned WebStates
// present.
TEST_F(SessionRestorationBrowserAgentTest,
       RestorePinnedWebStatesOnlyInSessionWithPinnedTabsDisabled) {
  CreateSessionRestorationBrowserAgent(false);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kPinnedOnly);

  ASSERT_EQ(7, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            regular_web_state_3);
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(3));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(4));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(6));
}

// Tests that restoring a session with scope `kRegularOnly` but disabled
// pinned tabs works correctly on non empty WebStatelist with pinned WebStates
// present.
TEST_F(SessionRestorationBrowserAgentTest,
       RestoreRegularWebStatesOnlyInSessionWithPinnedTabsDisabled) {
  CreateSessionRestorationBrowserAgent(false);

  web::WebState* pinned_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                        /*pinned=*/true, /*background=*/false);
  web::WebState* pinned_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                        /*pinned=*/true, /*background=*/false);

  web::WebState* regular_web_state_0 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/3,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_1 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/4,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_2 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/5,
                        /*pinned=*/false, /*background=*/false);
  web::WebState* regular_web_state_3 =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/6,
                        /*pinned=*/false, /*background=*/false);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 3,
                                         .tab_infos = {
                                             TabInfo{.pinned = true},
                                             TabInfo{.pinned = true},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kRegularOnly);

  ASSERT_EQ(12, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetActiveWebState(),
            browser_->GetWebStateList()->GetWebStateAt(10));
  EXPECT_EQ(pinned_web_state_0, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(pinned_web_state_1, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(pinned_web_state_2, browser_->GetWebStateList()->GetWebStateAt(2));
  EXPECT_EQ(regular_web_state_0, browser_->GetWebStateList()->GetWebStateAt(3));
  EXPECT_EQ(regular_web_state_1, browser_->GetWebStateList()->GetWebStateAt(4));
  EXPECT_EQ(regular_web_state_2, browser_->GetWebStateList()->GetWebStateAt(5));
  EXPECT_EQ(regular_web_state_3, browser_->GetWebStateList()->GetWebStateAt(6));
}

// TODO(crbug.com/888674): This test requires commiting item to
// NavigationManagerImpl which is not possible, migrate this to EG test so
// it can be tested.
TEST_F(SessionRestorationBrowserAgentTest, DISABLED_RestoreSessionOnNTPTest) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* web_state = InsertNewWebState(
      /*parent=*/nullptr, /*index=*/0, /*pinned=*/false, /*background=*/false);

  // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state);
  NewTabPageTabHelper::FromWebState(web_state)->SetDelegate(delegate);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<3>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(2),
            browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(1));
  EXPECT_NE(web_state, browser_->GetWebStateList()->GetWebStateAt(2));
}

// Tests that saving a non-empty session, then saving an empty session, then
// restoring, restores zero web states, and not the non-empty session.
TEST_F(SessionRestorationBrowserAgentTest, SaveAndRestoreEmptySession) {
  CreateSessionRestorationBrowserAgent(true);

  InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                    /*pinned=*/false,
                    /*background=*/false);

  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->SaveSession(/*immediately=*/true);
  [test_session_service_ setPerformIO:NO];

  // Session should be saved, now remove the webstate.
  browser_->GetWebStateList()->CloseWebStateAt(0, WebStateList::CLOSE_NO_FLAGS);
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
  session_restoration_agent_->RestoreSessionWindow(
      session_window, SessionRestorationScope::kAll);

  EXPECT_EQ(0, browser_->GetWebStateList()->count());
}

// Tests that saving a session with web states, then clearing the WebStatelist
// and then restoring the session will restore the web states correctly.
// TODO(crbug.com/1433670): The tests are flaky.
TEST_F(SessionRestorationBrowserAgentTest, DISABLED_SaveAndRestoreSession) {
  CreateSessionRestorationBrowserAgent(true);

  web::WebState* web_state =
      InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                        /*pinned=*/false, /*background=*/false);
  InsertNewWebState(web_state, /*index=*/1, /*pinned=*/false,
                    /*background=*/false);
  InsertNewWebState(web_state, /*index=*/0, /*pinned=*/false,
                    /*background=*/false);

  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  browser_->GetWebStateList()->ActivateWebStateAt(1);

  // Force state to flush to disk on the main thread so it can be immediately
  // tested below.
  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->SaveSession(/*immediately=*/true);
  [test_session_service_ setPerformIO:NO];
  // close all the webStates
  browser_->GetWebStateList()->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);

  const base::FilePath& state_path = chrome_browser_state_->GetStatePath();
  SessionIOS* session =
      [test_session_service_ loadSessionWithSessionID:session_id()
                                            directory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];

  // Restore from saved session.
  session_restoration_agent_->RestoreSessionWindow(
      session_window, SessionRestorationScope::kAll);

  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(1),
            browser_->GetWebStateList()->GetActiveWebState());
}

// Tests that saving a session with web states that are being restored, then
// clearing the WebStatelist and restoring the session will restore the web
// states correctly.
TEST_F(SessionRestorationBrowserAgentTest, SaveInProgressAndRestoreSession) {
  CreateSessionRestorationBrowserAgent(true);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<5>{.active_index = 1,
                                         .tab_infos = {
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  [test_session_service_ setPerformIO:YES];
  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);
  [test_session_service_ setPerformIO:NO];

  ASSERT_EQ(5, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(1),
            browser_->GetWebStateList()->GetActiveWebState());

  // Close all the webStates
  browser_->GetWebStateList()->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);

  const base::FilePath& state_path = chrome_browser_state_->GetStatePath();
  SessionIOS* session =
      [test_session_service_ loadSessionWithSessionID:session_id()
                                            directory:state_path];
  ASSERT_EQ(1u, session.sessionWindows.count);
  SessionWindowIOS* session_window = session.sessionWindows[0];
  session_restoration_agent_->RestoreSessionWindow(
      session_window, SessionRestorationScope::kAll);
  ASSERT_EQ(5, browser_->GetWebStateList()->count());
  EXPECT_EQ(browser_->GetWebStateList()->GetWebStateAt(1),
            browser_->GetWebStateList()->GetActiveWebState());
}

// Tests that SessionRestorationObserver methods are called when sessions is
// restored.
TEST_F(SessionRestorationBrowserAgentTest, ObserverCalledWithRestore) {
  CreateSessionRestorationBrowserAgent(true);

  InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                    /*pinned=*/false,
                    /*background=*/false);

  TestRestorationObserver observer;
  session_restoration_agent_->AddObserver(&observer);

  SessionWindowIOS* window =
      CreateSessionWindow(SessionInfo<3>{.active_index = 2,
                                         .tab_infos = {
                                             TabInfo{},
                                             TabInfo{},
                                             TabInfo{},
                                         }});

  session_restoration_agent_->RestoreSessionWindow(
      window, SessionRestorationScope::kAll);
  ASSERT_EQ(4, browser_->GetWebStateList()->count());

  EXPECT_TRUE(observer.restore_started());
  EXPECT_EQ(observer.restored_web_states_count(), 3);
  session_restoration_agent_->RemoveObserver(&observer);
}

// Tests that SessionRestorationAgent saves session when the active webState
// changes.
TEST_F(SessionRestorationBrowserAgentTest,
       SaveSessionWithActiveWebStateChange) {
  CreateSessionRestorationBrowserAgent(true);

  InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                    /*pinned=*/false,
                    /*background=*/true);
  InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                    /*pinned=*/false,
                    /*background=*/true);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 0);

  // Inserting new active webState.
  InsertNewWebState(/*parent=*/nullptr, /*index=*/2,
                    /*pinned=*/false,
                    /*background=*/false);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 1);

  // Removing the active webstate.
  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/2,
                                               WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 2);

  EXPECT_EQ(browser_->GetWebStateList()->active_index(), 1);

  // Activating another webState without removing or inserting.
  browser_->GetWebStateList()->ActivateWebStateAt(/*index=*/0);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 3);

  // Removing a non active webState.
  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/1,
                                               WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 4);

  // Removing the last active webState.
  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/0,
                                               WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 6);

  InsertNewWebState(/*parent=*/nullptr, /*index=*/0,
                    /*pinned=*/false,
                    /*background=*/true);
  InsertNewWebState(/*parent=*/nullptr, /*index=*/1,
                    /*pinned=*/false,
                    /*background=*/true);
  browser_->GetWebStateList()->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(test_session_service_.saveSessionCallsCount, 7);
}

}  // anonymous namespace
