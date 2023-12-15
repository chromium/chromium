// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/idle/idle_timeout_policy_utils.h"

#import "components/enterprise/idle/action_type.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_idle {

class IdleTimeoutPolicyUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    pref_service_ = browser_state_.get()->GetPrefs();
    authentication_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  void TearDown() override { browser_state_.reset(); }

  void SetIdleTimeoutActions(std::vector<ActionType> action_types) {
    base::Value::List actions;
    for (auto action_type : action_types) {
      actions.Append(static_cast<int>(action_type));
    }
    browser_state_->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                        std::move(actions));
  }

  void SignIn() {
    // Sign in.
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    authentication_service_->SignIn(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  PrefService* pref_service_;
  AuthenticationService* authentication_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_AllTypes_UserSignedIn) {
  SignIn();
  SetIdleTimeoutActions({ActionType::kSignOut, ActionType::kCloseTabs,
                         ActionType::kClearBrowsingHistory});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_TRUE(action_set.signout);
  EXPECT_TRUE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_AllTypes_UserSignedOut) {
  SetIdleTimeoutActions({ActionType::kSignOut, ActionType::kCloseTabs,
                         ActionType::kClearBrowsingHistory});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_TRUE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_CLEAR_DATA_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_CLEAR_DATA_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_Signout_UserSignedIn) {
  SignIn();
  SetIdleTimeoutActions({ActionType::kSignOut});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_FALSE(action_set.clear);
  EXPECT_TRUE(action_set.signout);
  EXPECT_FALSE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SIGNOUT_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SIGNOUT_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITHOUT_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_Signout_UserSignedOut) {
  SetIdleTimeoutActions({ActionType::kSignOut});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_FALSE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);

  // The string id getter functions are should be return nullopt for this case.
  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set), std::nullopt);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set), std::nullopt);
}

TEST_F(IdleTimeoutPolicyUtilsTest, AllActionsToActionSet_CloseTabs) {
  SetIdleTimeoutActions({ActionType::kCloseTabs});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_FALSE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_TRUE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITHOUT_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_ClearBrowsingHistory) {
  SetIdleTimeoutActions({ActionType::kClearBrowsingHistory});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest,
       ActionsToActionSet_ClearCookiesAndOtherSiteData) {
  SetIdleTimeoutActions({ActionType::kClearCookiesAndOtherSiteData});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);
}

TEST_F(IdleTimeoutPolicyUtilsTest,
       ActionsToActionSet_ClearCachedImagesAndFiles) {
  SetIdleTimeoutActions({ActionType::kClearCookiesAndOtherSiteData});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_ClearPasswordSignin) {
  SetIdleTimeoutActions({ActionType::kClearPasswordSignin});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_ClearAutofill) {
  SetIdleTimeoutActions({ActionType::kClearAutofill});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_FALSE(action_set.signout);
  EXPECT_FALSE(action_set.close);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_SignoutAndClearData) {
  SignIn();
  SetIdleTimeoutActions(
      {ActionType::kSignOut, ActionType::kClearBrowsingHistory});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_TRUE(action_set.clear);
  EXPECT_TRUE(action_set.signout);
  EXPECT_FALSE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_AND_SIGNOUT_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_AND_SIGNOUT_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA);
}

TEST_F(IdleTimeoutPolicyUtilsTest, ActionsToActionSet_SignoutAndCloseTabs) {
  SignIn();
  SetIdleTimeoutActions({ActionType::kSignOut, ActionType::kCloseTabs});
  ActionSet action_set = GetActionSet(pref_service_, authentication_service_);
  EXPECT_FALSE(action_set.clear);
  EXPECT_TRUE(action_set.signout);
  EXPECT_TRUE(action_set.close);

  EXPECT_EQ(GetIdleTimeoutActionsTitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_SIGNOUT_TITLE);
  EXPECT_EQ(GetIdleTimeoutActionsSnackbarMessageId(action_set),
            IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_SIGNOUT_SNACKBAR_MESSAGE);
  EXPECT_EQ(GetIdleTimeoutActionsSubtitleId(action_set),
            IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITHOUT_CLEAR_DATA);
}

}  // namespace enterprise_idle
