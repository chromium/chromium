// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace push_notification_settings {

class PushNotificationSettingsUtilTest : public PlatformTest {
 public:
  PushNotificationSettingsUtilTest() {
    TestProfileIOS* test_profile =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());

    const std::string profile_name = test_profile->GetProfileName();
    pref_service_ = test_profile->GetPrefs();

    manager_ = [[PushNotificationAccountContextManager alloc]
        initWithProfileManager:&profile_manager_];
    fake_id_ = [FakeSystemIdentity fakeIdentity1];
    // TODO(b/318863934): Remove flag when enabled by default.
    feature_list_.InitWithFeatures(
        {/*enabled=*/kContentNotificationExperiment, kIOSTipsNotifications,
         kSafetyCheckNotifications},
        {/*disabled=*/});
    AddTestCasesToManager(manager_, profile_attributes_storage(),
                          base::SysNSStringToUTF8(fake_id_.gaiaID),
                          profile_name);
  }
  ProfileAttributesStorageIOS* profile_attributes_storage() const {
    return GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage();
  }
  void TurnNotificationForKey(BOOL on,
                              const std::string key,
                              PrefService* pref_service) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(key, on);
  }
  void TurnAppLevelNotificationForKey(BOOL on, const std::string key) {
    PrefService* pref_service = GetApplicationContext()->GetLocalState();
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(key, on);
  }

  void TurnEmailNotifications(BOOL on, PrefService* pref_service) {
    pref_service->SetBoolean(commerce::kPriceEmailNotificationsEnabled, on);
  }
  void AddTestCasesToManager(PushNotificationAccountContextManager* manager,
                             ProfileAttributesStorageIOS* storage,
                             const std::string& gaia_id,
                             const std::string profile_name) {
    // Construct the Profiles with the given gaia id and add the gaia id
    // into the AccountContextManager.
    storage->UpdateAttributesForProfileWithName(
        profile_name,
        base::BindOnce(
            [](const std::string& gaia_id, ProfileAttributesIOS attr) {
              attr.SetAuthenticationInfo(gaia_id, /*user_name=*/std::string());
              return attr;
            },
            gaia_id));
    [manager addAccount:gaia_id];
  }

 protected:
  raw_ptr<PrefService> pref_service_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  FakeSystemIdentity* fake_id_;
  PushNotificationAccountContextManager* manager_;
  base::test::ScopedFeatureList feature_list_;
};

#pragma mark - All Clients Test

// Tests the overall permission state, which checks the notification state for
// all clients available on the device. This may include notification types
// other than push notifications.
// When 0 clients are enabled, the state is DISABLED.
// When `enabled` >= 1 AND `disabled` >= 1, the state is INDETERMINANT
// When `enabled` >=1 and `disabled` == 0, then state is ENABLED.
// TODO(crbug.com/370742354): re-enable
TEST_F(PushNotificationSettingsUtilTest, DISABLED_TestPermissionState) {
  // Enable Notifications in random order.
  ClientPermissionState state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnNotificationForKey(YES, kCommerceNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnEmailNotifications(YES, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnAppLevelNotificationForKey(YES, kSafetyCheckNotificationKey);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(YES, kContentNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnAppLevelNotificationForKey(YES, kTipsNotificationKey);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(YES, kSportsNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
  // Start disabling in a different order.
  TurnAppLevelNotificationForKey(NO, kTipsNotificationKey);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(NO, kContentNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnAppLevelNotificationForKey(NO, kSafetyCheckNotificationKey);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(NO, kCommerceNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnEmailNotifications(NO, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(NO, kSportsNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
}

#pragma mark - Price Tracking Notification Tests

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForCommerce) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnNotificationForKey(YES, kCommerceNotificationKey, pref_service_);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest,
       TestGetClientPermissionStateForCommerce) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kCommerce,
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnNotificationForKey(YES, kCommerceNotificationKey, pref_service_);
  state = GetClientPermissionState(PushNotificationClientId::kCommerce,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnEmailNotifications(YES, pref_service_);
  state = GetClientPermissionState(PushNotificationClientId::kCommerce,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

#pragma mark - Content Notification Tests

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForContent) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnNotificationForKey(YES, kContentNotificationKey, pref_service_);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest,
       TestGetClientPermissionStateForContent) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kContent,
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnNotificationForKey(YES, kContentNotificationKey, pref_service_);
  state = GetClientPermissionState(PushNotificationClientId::kContent,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForSports) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnNotificationForKey(YES, kSportsNotificationKey, pref_service_);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest,
       TestGetClientPermissionStateForSports) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kSports,
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnNotificationForKey(YES, kSportsNotificationKey, pref_service_);
  state = GetClientPermissionState(PushNotificationClientId::kSports,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

#pragma mark - Tips Notifications Tests

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForTips) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnAppLevelNotificationForKey(YES, kTipsNotificationKey);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest, TestGetClientPermissionStateForTips) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kTips, base::SysNSStringToUTF8(fake_id_.gaiaID),
      pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnAppLevelNotificationForKey(YES, kTipsNotificationKey);
  state = GetClientPermissionState(PushNotificationClientId::kTips,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

#pragma mark - Safety Check Notifications Tests

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForSafetyCheck) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnAppLevelNotificationForKey(YES, kSafetyCheckNotificationKey);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest,
       TestGetClientPermissionStateForSafetyCheck) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kSafetyCheck,
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnAppLevelNotificationForKey(YES, kSafetyCheckNotificationKey);
  state = GetClientPermissionState(PushNotificationClientId::kSafetyCheck,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

}  // namespace push_notification_settings
