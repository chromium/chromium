// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list.h"

#import "base/test/gtest_util.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/default_browser/utils_test_support.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing the SetUpList class.
class SetUpListTest : public PlatformTest {
 public:
  SetUpListTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  }

  // Builds a new instance of SetUpList.
  void BuildSetUpList() {
    set_up_list_ = [SetUpList buildFromPrefs:browser_state_->GetPrefs()
                       authenticationService:auth_service_];
  }

  // Fakes a sign-in with a fake identity.
  void SignInFakeIdentity() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    auth_service_->SignIn(identity);
    auth_service_->GrantSyncConsent(identity);
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Fakes enabling or disabling the credential provider.
  void FakeEnableCredentialProvider(bool enable) {
    password_manager_util::SetCredentialProviderEnabledOnStartup(
        browser_state_->GetPrefs(), enable);
  }

  // Returns a boolean indicating whether or not the built SetUpList includes
  // an item with the given `type`.
  bool ListIncludes(SetUpListItemType type) {
    for (SetUpListItem* item in set_up_list_.items) {
      if (item.type == type) {
        return true;
      }
    }
    return false;
  }

  // Expects the built SetUpList to include an item with the given `type`.
  void ExpectListToInclude(SetUpListItemType type) {
    EXPECT_TRUE(ListIncludes(type));
  }

  // Expects the built SetUpList to not include an item with the given `type`.
  void ExpectListToNotInclude(SetUpListItemType type) {
    EXPECT_FALSE(ListIncludes(type));
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationService* auth_service_;
  SetUpList* set_up_list_;
};

// Tests that the SetUpList uses the correct criteria when including the
// SyncInSync item.
TEST_F(SetUpListTest, buildListWithSignInSync) {
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kSignInSync);

  SignInFakeIdentity();
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);
}

// Tests that the SetUpList uses the correct criteria when including the
// DefaultBrowser item.
TEST_F(SetUpListTest, buildListWithDefaultBrowser) {
  SetFalseChromeLikelyDefaultBrowser();
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kDefaultBrowser);

  SetTrueChromeLikelyDefaultBrowser();
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kDefaultBrowser);
}

// Tests that the SetUpList uses the correct criteria when including the
// Autofill item.
TEST_F(SetUpListTest, buildListWithAutofill) {
  FakeEnableCredentialProvider(false);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kAutofill);

  FakeEnableCredentialProvider(true);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kAutofill);
}

// Tests that the SetUpList uses the correct criteria when including the
// Follow item.
TEST_F(SetUpListTest, buildListWithFollow) {
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kFollow);
}
