// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"

#import "base/files/file_path.h"
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
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace push_notification_settings {

class PushNotificationSettingsUtilTest : public PlatformTest {
 public:
  PushNotificationSettingsUtilTest() {
    test_chrome_browser_state_ = TestChromeBrowserState::Builder().Build();
    pref_service_ = test_chrome_browser_state_.get()->GetPrefs();
    default_browser_state_file_path_ =
        test_chrome_browser_state_->GetStatePath();
    test_manager_ = std::make_unique<TestChromeBrowserStateManager>(
        std::move(test_chrome_browser_state_));
    TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
        test_manager_.get());
    browser_state_info()->RemoveBrowserState(default_browser_state_file_path_);
    manager_ = [[PushNotificationAccountContextManager alloc]
        initWithChromeBrowserStateManager:test_manager_.get()];
    fake_id_ = [FakeSystemIdentity fakeIdentity1];
    // TODO(b/318863934): Remove flag when enabled by default.
    feature_list_.InitWithFeatures(
        {/*enabled=*/kContentPushNotifications, kIOSTipsNotifications},
        {/*disabled=*/});
    AddTestCasesToManager(manager_, browser_state_info(),
                          base::SysNSStringToUTF8(fake_id_.gaiaID),
                          default_browser_state_file_path_);
  }
  BrowserStateInfoCache* browser_state_info() const {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetBrowserStateInfoCache();
  }
  void TurnNotificationForKey(BOOL on,
                              const std::string key,
                              PrefService* pref_service) {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(key, on);
  }
  void TurnEmailNotifications(BOOL on, PrefService* pref_service) {
    pref_service->SetBoolean(commerce::kPriceEmailNotificationsEnabled, on);
  }
  void AddTestCasesToManager(PushNotificationAccountContextManager* manager,
                             BrowserStateInfoCache* info_cache,
                             const std::string& gaia_id,
                             base::FilePath path) {
    // Construct the BrowserStates with the given gaia id and add the gaia id
    // into the AccountContextManager.
    info_cache->AddBrowserState(path, gaia_id, std::u16string());
    [manager addAccount:gaia_id];
  }

 protected:
  PrefService* pref_service_;
  web::WebTaskEnvironment task_environment_;
  FakeSystemIdentity* fake_id_;
  PushNotificationAccountContextManager* manager_;
  std::unique_ptr<ChromeBrowserState> test_chrome_browser_state_;
  std::unique_ptr<ios::ChromeBrowserStateManager> test_manager_;
  base::FilePath default_browser_state_file_path_;
  base::test::ScopedFeatureList feature_list_;
};

#pragma mark - All Clients Test

// Tests the overall permission state, which checks the notification state for
// all clients available on the device. This may include notification types
// other than push notifications.
// When 0 clients are enabled, the state is DISABLED.
// When `enabled` >= 1 AND `disabled` >= 1, the state is INDETERMINANT
// When `enabled` >=1 and `disabled` == 0, then state is ENABLED.
TEST_F(PushNotificationSettingsUtilTest, TestPermissionState) {
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
  TurnNotificationForKey(YES, kContentNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(YES, kTipsNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
  // Start disabling in a different order.
  TurnNotificationForKey(NO, kTipsNotificationKey, pref_service_);
  state = GetNotificationPermissionState(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_EQ(ClientPermissionState::INDETERMINANT, state);
  TurnNotificationForKey(NO, kContentNotificationKey, pref_service_);
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

#pragma mark - Tips Notifications Tests

TEST_F(PushNotificationSettingsUtilTest,
       TestMobileNotificationsEnabledForTips) {
  BOOL isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_FALSE(isMobileNotificationsEnabled);
  TurnNotificationForKey(YES, kTipsNotificationKey, pref_service_);
  isMobileNotificationsEnabled = IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(fake_id_.gaiaID), pref_service_);
  EXPECT_TRUE(isMobileNotificationsEnabled);
}

TEST_F(PushNotificationSettingsUtilTest, TestGetClientPermissionStateForTips) {
  ClientPermissionState state = GetClientPermissionState(
      PushNotificationClientId::kTips, base::SysNSStringToUTF8(fake_id_.gaiaID),
      pref_service_);
  EXPECT_EQ(ClientPermissionState::DISABLED, state);
  TurnNotificationForKey(YES, kTipsNotificationKey, pref_service_);
  state = GetClientPermissionState(PushNotificationClientId::kTips,
                                   base::SysNSStringToUTF8(fake_id_.gaiaID),
                                   pref_service_);
  EXPECT_EQ(ClientPermissionState::ENABLED, state);
}

}  // namespace push_notification_settings
