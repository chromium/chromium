// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/gtest_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
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
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    prefs_ = GetProfile()->GetPrefs();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        GetProfile(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = AuthenticationServiceFactory::GetForProfile(GetProfile());
    content_notification_feature_enabled_ = false;
  }

  ~SetUpListTest() override { [set_up_list_ disconnect]; }

  // Get the test profile.
  ProfileIOS* GetProfile() { return profile_.get(); }

  // Get the LocalState prefs.
  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  // Builds a new instance of SetUpList.
  void BuildSetUpList() {
    [set_up_list_ disconnect];
    set_up_list_ =
        [SetUpList buildFromPrefs:prefs_
                            localState:GetLocalState()
                           syncService:SyncServiceFactory::GetForProfile(
                                           GetProfile())
                 authenticationService:auth_service_
            contentNotificationEnabled:content_notification_feature_enabled_];
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

    profile_manager_.GetProfileAttributesStorage()
        ->UpdateAttributesForProfileWithName(
            profile_->GetProfileName(),
            base::BindOnce(
                [](id<SystemIdentity> identity, ProfileAttributesIOS attr) {
                  attr.SetAuthenticationInfo(
                      base::SysNSStringToUTF8(identity.gaiaID),
                      base::SysNSStringToUTF8(identity.userEmail));
                  return attr;
                },
                identity));
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Fakes enabling or disabling the credential provider.
  void FakeEnableCredentialProvider(bool enable) {
    password_manager_util::SetCredentialProviderEnabledOnStartup(
        GetLocalState(), enable);
  }

  // Enables/disables tips notifications.
  void SetTipsNotificationsEnabled(bool enable) {
    ScopedDictPrefUpdate update(GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kTipsNotificationKey, enable);
  }

  // Enables/disables content notifications.
  void SetContentNotificationsEnabled(bool enable) {
    ScopedDictPrefUpdate update(prefs_,
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(kContentNotificationKey, enable);
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
    return set_up_list_prefs::GetItemState(GetLocalState(), type);
  }

  // Sets the state of the item with the given `type` from prefs.
  void SetItemState(SetUpListItemType type, SetUpListItemState state) {
    set_up_list_prefs::SetItemState(GetLocalState(), type, state);
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
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<AuthenticationService> auth_service_;
  SetUpList* set_up_list_;
  bool content_notification_feature_enabled_;
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
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);
  // Re-enable signin policy.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
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
            SetUpListItemState::kCompleteInList);

  SetItemState(SetUpListItemType::kSignInSync,
               SetUpListItemState::kCompleteNotInList);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kSignInSync);
  EXPECT_EQ(GetItemState(SetUpListItemType::kSignInSync),
            SetUpListItemState::kCompleteNotInList);
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
            SetUpListItemState::kCompleteInList);

  SetItemState(SetUpListItemType::kDefaultBrowser,
               SetUpListItemState::kCompleteNotInList);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kDefaultBrowser);
  EXPECT_EQ(GetItemState(SetUpListItemType::kDefaultBrowser),
            SetUpListItemState::kCompleteNotInList);
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
            SetUpListItemState::kCompleteInList);

  SetItemState(SetUpListItemType::kAutofill,
               SetUpListItemState::kCompleteNotInList);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kAutofill);
  EXPECT_EQ(GetItemState(SetUpListItemType::kAutofill),
            SetUpListItemState::kCompleteNotInList);
}

// Tests that the SetUpList uses the correct criteria when including the
// Notifications item and tips notification is enabled.
TEST_F(SetUpListTest, BuildListWithNotifications_Tips) {
  feature_list_.InitAndEnableFeature(kIOSTipsNotifications);
  SetTipsNotificationsEnabled(false);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kNotifications, NO);

  SetTipsNotificationsEnabled(true);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kNotifications, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kNotifications),
            SetUpListItemState::kCompleteInList);

  SetItemState(SetUpListItemType::kNotifications,
               SetUpListItemState::kCompleteNotInList);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kNotifications);
  EXPECT_EQ(GetItemState(SetUpListItemType::kNotifications),
            SetUpListItemState::kCompleteNotInList);
}

// Tests that the SetUpList uses the correct criteria when including the
// Notifications item and content notifications is enabled.
TEST_F(SetUpListTest, BuildListWithNotifications_Content) {
  content_notification_feature_enabled_ = YES;

  SetContentNotificationsEnabled(false);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kNotifications, NO);

  SetContentNotificationsEnabled(true);
  BuildSetUpList();
  ExpectListToInclude(SetUpListItemType::kNotifications, YES);
  EXPECT_EQ(GetItemState(SetUpListItemType::kNotifications),
            SetUpListItemState::kCompleteInList);

  SetItemState(SetUpListItemType::kNotifications,
               SetUpListItemState::kCompleteNotInList);
  BuildSetUpList();
  ExpectListToNotInclude(SetUpListItemType::kNotifications);
  EXPECT_EQ(GetItemState(SetUpListItemType::kNotifications),
            SetUpListItemState::kCompleteNotInList);
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
  OCMExpect([delegate setUpListItemDidComplete:item allItemsCompleted:NO]);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kSignInSync);
  EXPECT_TRUE(item.complete);
  [delegate verify];
}

// Tests that `allItemsComplete` correctly returns whether all items are
// complete.
TEST_F(SetUpListTest, AllItemsComplete) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(kIOSTipsNotifications);
  BuildSetUpList();
  EXPECT_FALSE([set_up_list_ allItemsComplete]);
  histogram_tester.ExpectBucketCount("IOS.SetUpList.AllItemsCompleted", true,
                                     0);

  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kSignInSync);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kDefaultBrowser);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kAutofill);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kNotifications);

  EXPECT_TRUE([set_up_list_ allItemsComplete]);
  histogram_tester.ExpectBucketCount("IOS.SetUpList.AllItemsCompleted", true,
                                     1);
}

TEST_F(SetUpListTest, RecordsAllItemsCompleteOnce) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(kIOSTipsNotifications);
  BuildSetUpList();
  histogram_tester.ExpectBucketCount("IOS.SetUpList.AllItemsCompleted", true,
                                     0);

  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kSignInSync);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kDefaultBrowser);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kAutofill);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kNotifications);
  histogram_tester.ExpectBucketCount("IOS.SetUpList.AllItemsCompleted", true,
                                     1);

  // Ensure that this metric is not double-counted, when rebuilding the list.
  BuildSetUpList();
  histogram_tester.ExpectBucketCount("IOS.SetUpList.AllItemsCompleted", true,
                                     1);
}

// Tests that the Set Up List can be disabled.
TEST_F(SetUpListTest, Disable) {
  EXPECT_FALSE(set_up_list_prefs::IsSetUpListDisabled(GetLocalState()));
  set_up_list_prefs::DisableSetUpList(GetLocalState());
  EXPECT_TRUE(set_up_list_prefs::IsSetUpListDisabled(GetLocalState()));

  BuildSetUpList();
  EXPECT_EQ(set_up_list_, nil);
}

// Tests that the Set Up List item order is correct with kMagicStack enabled.
TEST_F(SetUpListTest, MagicStackItemOrder) {
  feature_list_.InitWithFeatures({kIOSTipsNotifications}, {});
  BuildSetUpList();

  EXPECT_EQ(GetItemIndex(SetUpListItemType::kDefaultBrowser), 0u);
  EXPECT_EQ(GetItemIndex(SetUpListItemType::kAutofill), 1u);
  EXPECT_EQ(GetItemIndex(SetUpListItemType::kNotifications), 2u);
  EXPECT_EQ(GetItemIndex(SetUpListItemType::kSignInSync), 3u);
}
