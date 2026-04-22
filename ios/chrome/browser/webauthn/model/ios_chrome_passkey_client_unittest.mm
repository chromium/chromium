// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::Return;

class IOSChromePasskeyClientTest : public PlatformTest {
 public:
  IOSChromePasskeyClientTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();

    fake_web_state_.SetBrowserState(profile_.get());

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    client_ = std::make_unique<IOSChromePasskeyClient>(&fake_web_state_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState fake_web_state_;
  raw_ptr<syncer::MockSyncService> sync_service_mock_ = nullptr;
  std::unique_ptr<IOSChromePasskeyClient> client_;
};

// Tests that passkey saving is enabled when all conditions are met.
TEST_F(IOSChromePasskeyClientTest, EnabledWhenAllConditionsMet) {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, true);

  EXPECT_CALL(*(sync_service_mock_->GetMockUserSettings()), GetSelectedTypes())
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kPasswords})));

  EXPECT_TRUE(client_->IsGpmPasskeySavingEnabled());
}

// Tests that passkey saving is disabled when password manager is disabled.
TEST_F(IOSChromePasskeyClientTest, PasswordManagerDisabled) {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, true);

  EXPECT_CALL(*(sync_service_mock_->GetMockUserSettings()), GetSelectedTypes())
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kPasswords})));

  EXPECT_FALSE(client_->IsGpmPasskeySavingEnabled());
}

// Tests that passkey saving is disabled when passkeys are disabled.
TEST_F(IOSChromePasskeyClientTest, PasskeysDisabled) {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, false);

  EXPECT_CALL(*(sync_service_mock_->GetMockUserSettings()), GetSelectedTypes())
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kPasswords})));

  EXPECT_FALSE(client_->IsGpmPasskeySavingEnabled());
}

// Tests that passkey saving is disabled when password sync is disabled.
TEST_F(IOSChromePasskeyClientTest, PasswordSyncDisabled) {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, true);

  EXPECT_CALL(*(sync_service_mock_->GetMockUserSettings()), GetSelectedTypes())
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet()));

  EXPECT_FALSE(client_->IsGpmPasskeySavingEnabled());
}

// Tests that passkey saving is disabled when sync service is null.
TEST_F(IOSChromePasskeyClientTest, SyncServiceNull) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
        return nullptr;
      }));
  auto profile = std::move(builder).Build();
  web::FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(profile.get());
  IOSChromePasskeyClient client(&fake_web_state);

  EXPECT_FALSE(client.IsGpmPasskeySavingEnabled());
}

// Tests that passkey saving is enabled in Incognito if the original profile has
// sync enabled.
TEST_F(IOSChromePasskeyClientTest, IncognitoOriginalSyncEnabled) {
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);
  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, true);

  EXPECT_CALL(*(sync_service_mock_->GetMockUserSettings()), GetSelectedTypes())
      .WillRepeatedly(Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kPasswords})));

  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();

  web::FakeWebState incognito_web_state;
  incognito_web_state.SetBrowserState(incognito_profile);
  IOSChromePasskeyClient incognito_client(&incognito_web_state);

  EXPECT_TRUE(incognito_client.IsGpmPasskeySavingEnabled());
}

}  // namespace
