// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/action.h"

#import "base/memory/raw_ptr.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::_;

namespace enterprise_idle {

class IdleActionTest : public PlatformTest {
 protected:
  using ActionQueue = ActionFactory::ActionQueue;
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile(), std::make_unique<FakeAuthenticationServiceDelegate>());
    main_browsing_data_remover_ = std::make_unique<FakeBrowsingDataRemover>();
    incognito_browsing_data_remover_ =
        std::make_unique<FakeBrowsingDataRemover>();
    action_factory_ = std::make_unique<ActionFactory>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    main_browsing_data_remover_.reset();
    incognito_browsing_data_remover_.reset();
    profile_.reset();
  }

  ActionFactory::ActionQueue GetActions(std::vector<ActionType> action_types) {
    return action_factory_->Build(action_types, main_remover(),
                                  incognito_remover());
  }

  void SignIn() {
    authentication_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetForProfile(profile()));
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    authentication_service_->SignIn(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Inserts WebStates into `browser` each one loading a new URL from `urls`
  // and wait until all the WebStates are done with the navigation.
  void InsertTabs() {
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile());
    browser_list->AddBrowser(browser_.get());
    browser_list->AddBrowser(incognito_browser_.get());

    // Insert some web states in each browser.
    std::vector<std::string> urls{"https://foo/bar", "https://car/tar",
                                  "https://hello/world"};
    std::vector<web::WebStateID> identifiers;
    for (int i = 0; i < 3; i++) {
      auto web_state = CreateFakeWebStateWithURL(GURL(urls[i]));
      auto incognito_web_state = CreateFakeWebStateWithURL(GURL(urls[i]));
      browser_->GetWebStateList()->InsertWebState(
          std::move(web_state), WebStateList::InsertionParams::AtIndex(i));
      incognito_browser_->GetWebStateList()->InsertWebState(
          std::move(incognito_web_state),
          WebStateList::InsertionParams::AtIndex(i));
    }
  }

  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(profile());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    return web_state;
  }

  int GetTabsCount(TestBrowser* browser) {
    return browser->GetWebStateList()->count();
  }

  TestProfileIOS* profile() { return profile_.get(); }

  FakeBrowsingDataRemover* main_remover() {
    return main_browsing_data_remover_.get();
  }

  FakeBrowsingDataRemover* incognito_remover() {
    return incognito_browsing_data_remover_.get();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  raw_ptr<AuthenticationService> authentication_service_;
  // ScopedTestingLocalState needed for the authentication service.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ActionFactory> action_factory_;
  std::unique_ptr<FakeBrowsingDataRemover> main_browsing_data_remover_;
  std::unique_ptr<FakeBrowsingDataRemover> incognito_browsing_data_remover_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(IdleActionTest, ClearBrowsingHistory) {
  ActionQueue actions = GetActions({ActionType::kClearBrowsingHistory});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, ClearCookies) {
  ActionQueue actions = GetActions({ActionType::kClearCookiesAndOtherSiteData});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_SITE_DATA,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_SITE_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, ClearCache) {
  ActionQueue actions = GetActions({ActionType::kClearCachedImagesAndFiles});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_CACHE,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_CACHE,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, ClearPasswordSignin) {
  ActionQueue actions = GetActions({ActionType::kClearPasswordSignin});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_PASSWORDS,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_PASSWORDS,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, ClearAutofill) {
  ActionQueue actions = GetActions({ActionType::kClearAutofill});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            main_remover()->GetLastUsedRemovalMask());
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, MultipleTypesAndSuccess) {
  ActionQueue actions = GetActions(
      {ActionType::kClearBrowsingHistory, ActionType::kClearAutofill});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY |
                BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY |
                BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionTest, MultipleTypesAndFailure) {
  main_remover()->SetFailedForTesting();
  ActionQueue actions = GetActions(
      {ActionType::kClearBrowsingHistory, ActionType::kClearAutofill});
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(static_cast<int>(ActionType::kClearBrowsingHistory),
            actions.top()->priority());

  // The callback should run with success=false.
  base::MockCallback<Action::Continuation> continuation;
  base::RunLoop run_loop;
  EXPECT_CALL(continuation, Run(/*success=*/false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  actions.top()->Run(profile(), continuation.Get());
  run_loop.Run();
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", false, 1);
}

TEST_F(IdleActionTest, SignOut) {
  ActionQueue actions = GetActions({ActionType::kSignOut});
  base::MockCallback<Action::Continuation> continuation;
  // Check that the right action is added.
  EXPECT_EQ(static_cast<int>(ActionType::kSignOut), actions.top()->priority());
  SignIn();
  ASSERT_TRUE(authentication_service_->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  base::RunLoop run_loop;
  // The test needs to wait for the call so that the action is not removed
  // before sign out completes.
  EXPECT_CALL(continuation, Run(/*success=*/true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  actions.top()->Run(profile(), continuation.Get());
  run_loop.Run();
  ASSERT_FALSE(authentication_service_->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.SignOut", true, 1);
}

TEST_F(IdleActionTest, CloseTabs) {
  ActionQueue actions = GetActions({ActionType::kCloseTabs});
  // Check that the right action is added.
  EXPECT_EQ(static_cast<int>(ActionType::kCloseTabs),
            actions.top()->priority());
  // Insert 3 tabs in each browser, and check that the tab count to verify that
  // they have been added.
  InsertTabs();
  EXPECT_EQ(GetTabsCount(browser_.get()), 3);
  EXPECT_EQ(GetTabsCount(incognito_browser_.get()), 3);
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(profile(), continuation.Get());
  // Tabs in both regular and incognito browser should all be closed after
  // CloseTabsAction runs.
  EXPECT_EQ(GetTabsCount(browser_.get()), 0);
  EXPECT_EQ(GetTabsCount(incognito_browser_.get()), 0);
  actions.pop();
  histogram_tester_->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.CloseTabs", true, 1);
}

}  // namespace enterprise_idle
