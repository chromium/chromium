// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list.h"

#import "base/test/gtest_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ntp/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using set_up_list_prefs::SetUpListItemState;

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
    prefs_ = browser_state_->GetPrefs();
  }

  ~SetUpListTest() override { [set_up_list_ disconnect]; }

  // Builds a new instance of SetUpList.
  void BuildSetUpList() {
    [set_up_list_ disconnect];
    set_up_list_ =
        [SetUpList buildFromPrefs:prefs_
                       localState:local_state_.Get()
                      syncService:SyncServiceFactory::GetForBrowserState(
                                      browser_state_.get())
            authenticationService:auth_service_];
  }

  // Fakes a sign-in with a fake identity.
  void SignInFakeIdentity() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    auth_service_->SignIn(identity,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
    auth_service_->GrantSyncConsent(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
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

  // Returns the item with the given `type`. Returns nil if not found.
  SetUpListItem* FindItem(SetUpListItemType type) {
    for (SetUpListItem* item in set_up_list_.items) {
      if (item.type == type) {
        return item;
      }
    }
    return nil;
  }

  // Expects the built SetUpList to include an item with the given `type` and
  // expects it to have the given `complete` status.
  void ExpectListToInclude(SetUpListItemType type, BOOL complete) {
    SetUpListItem* item = FindItem(type);
    EXPECT_TRUE(item);
    EXPECT_EQ(item.complete, complete);
  }

  // Expects the built SetUpList to not include an item with the given `type`.
  void ExpectListToNotInclude(SetUpListItemType type) {
    SetUpListItem* item = FindItem(type);
    EXPECT_EQ(item, nil);
  }

  // Gets the state of the item with the given `type` from prefs.
  SetUpListItemState GetItemState(SetUpListItemType type) {
    return set_up_list_prefs::GetItemState(local_state_.Get(), type);
  }

  // Sets the state of the item with the given `type` from prefs.
  void SetItemState(SetUpListItemType type, SetUpListItemState state) {
    set_up_list_prefs::SetItemState(local_state_.Get(), type, state);
  }

  NSUInteger GetItemIndex(SetUpListItemType type) {
    for (NSUInteger i = 0; i < [set_up_list_.items count]; i++) {
      if (set_up_list_.items[i].type == type) {
        return i;
      }
    }
    return -1;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  PrefService* prefs_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationService* auth_service_;
  SetUpList* set_up_list_;
};

// Tests the SignInSync item is hidden if sync is disabled by policy.
TEST_F(SetUpListTest, NoSignInSyncIfSyncDisabledByPolicy) {
  prefs_->SetBoolean(syncer::prefs::internal::kSyncManaged, true);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);

  prefs_->ClearPref(syncer::prefs::internal::kSyncManaged);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kSignInSync, NO);
}

// Tests the SignInSync item is hidden if sign-in is disabled by policy.
TEST_F(SetUpListTest, NoSignInSyncItemIfSigninDisabledByPolicy) {
  // Set sign-in disabled by policy.
  local_state_.Get()->SetInteger(
      prefs::kBrowserSigninPolicy,
      static_cast<int>(BrowserSigninMode::kDisabled));
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);
  // Re-enable signin policy.
  local_state_.Get()->SetInteger(prefs::kBrowserSigninPolicy,
                                 static_cast<int>(BrowserSigninMode::kEnabled));
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kSignInSync, NO);
}

// Tests that the SetUpList shows or hides the SignInSync item depending on
// whether the user is currently signed-in.
TEST_F(SetUpListTest, SignInSyncReactsToAccountChanges) {
  SignInFakeIdentity();
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kSignInSync, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kSignInSync),
            SetUpListItemState::kCompleteNotInList);

  SetItemState(SetUpListItemType::kSignInSync,
               SetUpListItemState::kCompleteInList);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kSignInSync, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kSignInSync),
            SetUpListItemState::kCompleteNotInList);

  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);
}

// Tests that the SetUpList uses the correct criteria when including the
// DefaultBrowser item.
TEST_F(SetUpListTest, BuildListWithDefaultBrowser) {
  SetFalseChromeLikelyDefaultBrowser();
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kDefaultBrowser, NO);

  SetTrueChromeLikelyDefaultBrowser();
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kDefaultBrowser, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kDefaultBrowser),
            SetUpListItemState::kCompleteNotInList);

  SetItemState(SetUpListItemType::kDefaultBrowser,
               SetUpListItemState::kCompleteInList);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kDefaultBrowser, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kDefaultBrowser),
            SetUpListItemState::kCompleteNotInList);

  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kDefaultBrowser);
}

// Tests that the SetUpList uses the correct criteria when including the
// Autofill item.
TEST_F(SetUpListTest, BuildListWithAutofill) {
  FakeEnableCredentialProvider(false);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kAutofill, NO);

  FakeEnableCredentialProvider(true);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kAutofill, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kAutofill),
            SetUpListItemState::kCompleteNotInList);

  SetItemState(SetUpListItemType::kAutofill,
               SetUpListItemState::kCompleteInList);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kAutofill, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kAutofill),
            SetUpListItemState::kCompleteNotInList);

  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kAutofill);
}

// Tests that the SetUpList uses the correct criteria when including the
// Follow item.
TEST_F(SetUpListTest, BuildListWithFollow) {
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kFollow);
}

// Tests that SetUpList observes local state changes, updates the item, and
// calls the delegate.
TEST_F(SetUpListTest, ObservesPrefs) {
  BuildSetUpList();
  id delegate = [OCMockObject mockForProtocol:@protocol(SetUpListDelegate)];
  set_up_list_.delegate = delegate;
  SetUpListItem* item = FindItem(SetUpListItemType::kSignInSync);
  EXPECT_FALSE(item.complete);
  OCMExpect([delegate setUpListItemDidComplete:item]);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kSignInSync);
  EXPECT_TRUE(item.complete);
  [delegate verify];
}

// Tests that `allItemsComplete` correctly returns whether all items are
// complete.
TEST_F(SetUpListTest, AllItemsComplete) {
  BuildSetUpList();
  EXPECT_FALSE([set_up_list_ allItemsComplete]);

  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kSignInSync);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kDefaultBrowser);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kAutofill);

  EXPECT_TRUE([set_up_list_ allItemsComplete]);
}

// Tests that the Set Up List can be disabled.
TEST_F(SetUpListTest, Disable) {
  EXPECT_FALSE(set_up_list_prefs::IsSetUpListDisabled(local_state_.Get()));
  set_up_list_prefs::DisableSetUpList(local_state_.Get());
  EXPECT_TRUE(set_up_list_prefs::IsSetUpListDisabled(local_state_.Get()));

  BuildSetUpList();
  EXPECT_EQ(set_up_list_, nil);
}

// Tests that the Set Up List item order is correct with kMagicStack enabled.
TEST_F(SetUpListTest, MagicStackItemOrder) {
  feature_list_.InitWithFeatures({kMagicStack}, {});
  BuildSetUpList();

  EXPECT_EQ(GetItemIndex(SetUpListItemType::kDefaultBrowser), 0u);
  EXPECT_EQ(GetItemIndex(SetUpListItemType::kAutofill), 1u);
  EXPECT_EQ(GetItemIndex(SetUpListItemType::kSignInSync), 2u);
}
