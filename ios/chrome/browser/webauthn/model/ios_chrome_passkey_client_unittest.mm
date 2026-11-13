// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import <cstdint>
#import <memory>
#import <string>
#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/protobuf_matchers.h"
#import "base/test/test_future.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/core/browser/device_authorization/mock_device_authorization_service.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import <optional>

#import "base/test/run_until.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/browser/credential_provider/model/fake_credential_provider_settings_handler.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)

namespace {

using ::base::test::EqualsProto;
using ::base::test::RunOnceCallback;
using ::testing::Return;

constexpr int32_t kDeviceAuthorizationKeyVersion = 1;
constexpr std::string_view kDeviceAuthorizationKey = "device_authorization_key";

std::unique_ptr<KeyedService> BuildMockDeviceAuthorizationService(
    ProfileIOS* profile) {
  return std::make_unique<webauthn::MockDeviceAuthorizationService>();
}

class IOSChromePasskeyClientTest : public PlatformTest {
 public:
  IOSChromePasskeyClientTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        IOSDeviceAuthorizationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockDeviceAuthorizationService));
    profile_ = std::move(builder).Build();

    fake_web_state_.SetBrowserState(profile_.get());

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    device_authorization_service_ =
        static_cast<webauthn::MockDeviceAuthorizationService*>(
            IOSDeviceAuthorizationServiceFactory::GetForProfile(
                profile_.get()));

    client_ = std::make_unique<IOSChromePasskeyClient>(&fake_web_state_);
  }

  ~IOSChromePasskeyClientTest() override = default;

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  struct RegisteredBrowser {
    FakeCredentialProviderSettingsHandler* settings_handler = nil;
    std::unique_ptr<TestBrowser> browser;
  };

  RegisteredBrowser CreateRegisteredBrowser(bool incognito) {
    RegisteredBrowser registered;
    registered.settings_handler =
        [[FakeCredentialProviderSettingsHandler alloc] init];
    registered.browser = std::make_unique<TestBrowser>(
        incognito ? profile_->GetOffTheRecordProfile() : profile_.get());
    [registered.settings_handler
        registerWithCommandDispatcher:registered.browser
                                          ->GetCommandDispatcher()];
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(registered.browser.get());
    return registered;
  }

  web::FakeWebState* AppendWebState(Browser* browser, bool is_visible) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser->GetProfile());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    web::FakeWebState* raw_web_state = web_state.get();
    InfoBarManagerImpl::CreateForWebState(raw_web_state);
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    if (is_visible) {
      raw_web_state->WasShown();
    }
    return raw_web_state;
  }
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState fake_web_state_;
  raw_ptr<syncer::MockSyncService> sync_service_mock_ = nullptr;
  raw_ptr<webauthn::MockDeviceAuthorizationService>
      device_authorization_service_ = nullptr;
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

// Tests that the credential provider promo is shown on passkey creation if the
// credential provider is not enabled on startup.
TEST_F(IOSChromePasskeyClientTest, ShowPromoOnPasskeyCreated) {
  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, false);

  id mock_commands_handler =
      OCMProtocolMock(@protocol(IOSPasskeyClientCommands));
  OCMExpect(
      [mock_commands_handler showCredentialProviderPromoOnPasskeyCreated]);

  client_->SetIOSPasskeyClientCommandsHandler(mock_commands_handler);
  client_->OnPasskeyCreated();

  [mock_commands_handler verify];
}

// Tests that the credential provider promo is not shown on passkey creation if
// the credential provider is already enabled on startup.
TEST_F(IOSChromePasskeyClientTest, DoNotShowPromoOnPasskeyCreated) {
  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, true);

  id mock_commands_handler =
      OCMProtocolMock(@protocol(IOSPasskeyClientCommands));
  [[mock_commands_handler reject] showCredentialProviderPromoOnPasskeyCreated];

  client_->SetIOSPasskeyClientCommandsHandler(mock_commands_handler);
  client_->OnPasskeyCreated();

  [mock_commands_handler verify];
}

// Tests that automatic passkey upgrade is enabled by default and follows the
// pref value.
TEST_F(IOSChromePasskeyClientTest, AutomaticPasskeyUpgradePref) {
  EXPECT_TRUE(client_->IsAutomaticPasskeyUpgradeEnabled());

  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kAutomaticPasskeyUpgrades, false);
  EXPECT_FALSE(client_->IsAutomaticPasskeyUpgradeEnabled());

  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kAutomaticPasskeyUpgrades, true);
  EXPECT_TRUE(client_->IsAutomaticPasskeyUpgradeEnabled());
}

// Tests that the fetched device authorization keys are passed to the caller.
TEST_F(IOSChromePasskeyClientTest, FetchDeviceAuthorizationKeys) {
  webauthn::DeviceAuthorizationKeys keys;
  webauthn::DeviceAuthorizationKey* key = keys.add_keys();
  key->set_version(kDeviceAuthorizationKeyVersion);
  key->set_key(kDeviceAuthorizationKey);
  EXPECT_CALL(*device_authorization_service_, GetOrFetchKeys)
      .WillOnce(RunOnceCallback<0>(webauthn::DeviceAuthFetchResult(keys)));

  base::test::TestFuture<webauthn::DeviceAuthFetchResult> future;
  client_->FetchDeviceAuthorizationKeys(future.GetCallback());

  const webauthn::DeviceAuthorizationKeys* fetched_keys = future.Get().keys();
  ASSERT_TRUE(fetched_keys);
  EXPECT_THAT(*fetched_keys, EqualsProto(keys));
}

// Tests that fetching device authorization keys fails when the device
// authorization service is null.
TEST_F(IOSChromePasskeyClientTest, DeviceAuthorizationServiceNull) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      IOSDeviceAuthorizationServiceFactory::GetInstance(),
      base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
        return nullptr;
      }));
  auto profile = std::move(builder).Build();
  web::FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(profile.get());
  IOSChromePasskeyClient client(&fake_web_state);

  base::test::TestFuture<webauthn::DeviceAuthFetchResult> future;
  client.FetchDeviceAuthorizationKeys(future.GetCallback());

  EXPECT_EQ(future.Get().status(),
            webauthn::DeviceAuthFetchResult::Status::kError);
}

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
// Tests that `ShowPasskeyCreationInfobar` displays an infobar only on the
// `Browser` whose `WebStateList` owns `web_state_`, leaving other windows
// untouched, and routes `Accept()` to the owning `Browser`'s
// `SettingsCommands` handler.
TEST_F(IOSChromePasskeyClientTest,
       ShowPasskeyCreationInfobarScopesToOwningBrowser) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_.get()), "user@example.com",
      signin::ConsentLevel::kSignin);

  RegisteredBrowser browser_1 = CreateRegisteredBrowser(/*incognito=*/false);
  RegisteredBrowser browser_2 = CreateRegisteredBrowser(/*incognito=*/false);
  web::FakeWebState* web_state_1 =
      AppendWebState(browser_1.browser.get(), /*is_visible=*/true);
  web::FakeWebState* web_state_2 =
      AppendWebState(browser_2.browser.get(), /*is_visible=*/true);

  IOSChromePasskeyClient client_1(web_state_1);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id("example.com");
  passkey.set_credential_id("credential_id");
  client_1.ShowPasskeyCreationInfobar(passkey);

  InfoBarManagerImpl* infobar_manager_1 =
      InfoBarManagerImpl::FromWebState(web_state_1);
  ASSERT_EQ(1u, infobar_manager_1->infobars().size());
  ConfirmInfoBarDelegate* confirm_delegate =
      infobar_manager_1->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_delegate);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                u"user@example.com"),
            confirm_delegate->GetMessageText());
  EXPECT_EQ(0u,
            InfoBarManagerImpl::FromWebState(web_state_2)->infobars().size());

  EXPECT_TRUE(confirm_delegate->Accept());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser_1.settings_handler.showPasswordDetailsCallCount == 1u;
  }));

  EXPECT_FALSE(browser_1.settings_handler.calledInEditMode);
  ASSERT_TRUE(browser_1.settings_handler.lastCredential.has_value());
  ASSERT_TRUE(browser_1.settings_handler.lastCredential->IsPasskey());
  EXPECT_EQ("example.com",
            browser_1.settings_handler.lastCredential->passkey_info->rp_id);
  EXPECT_EQ(0u, browser_2.settings_handler.showPasswordDetailsCallCount);
}

// Tests that `ShowPasskeyCreationInfobar` resolves an Incognito `Browser` from
// the original profile's `BrowserList` and formats the original profile's
// primary account email.
TEST_F(IOSChromePasskeyClientTest,
       ShowPasskeyCreationInfobarIncognitoBrowserUsesOriginalProfileEmail) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_.get()), "user@example.com",
      signin::ConsentLevel::kSignin);

  RegisteredBrowser regular_browser =
      CreateRegisteredBrowser(/*incognito=*/false);
  RegisteredBrowser incognito_browser =
      CreateRegisteredBrowser(/*incognito=*/true);
  web::FakeWebState* regular_web_state =
      AppendWebState(regular_browser.browser.get(), /*is_visible=*/true);
  web::FakeWebState* incognito_web_state =
      AppendWebState(incognito_browser.browser.get(), /*is_visible=*/true);

  IOSChromePasskeyClient incognito_client(incognito_web_state);
  sync_pb::WebauthnCredentialSpecifics passkey;
  incognito_client.ShowPasskeyCreationInfobar(passkey);

  EXPECT_EQ(
      0u,
      InfoBarManagerImpl::FromWebState(regular_web_state)->infobars().size());
  InfoBarManagerImpl* incognito_infobar_manager =
      InfoBarManagerImpl::FromWebState(incognito_web_state);
  ASSERT_EQ(1u, incognito_infobar_manager->infobars().size());
  ConfirmInfoBarDelegate* confirm_delegate =
      incognito_infobar_manager->infobars()[0]
          ->delegate()
          ->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_delegate);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                u"user@example.com"),
            confirm_delegate->GetMessageText());
}

// Tests that `ShowPasskeyCreationInfobar` does not add an infobar when no
// primary account is signed in (e.g., if sign-out occurred while async key
// fetching was in flight).
TEST_F(IOSChromePasskeyClientTest, ShowPasskeyCreationInfobarSignedOutNoOp) {
  RegisteredBrowser browser = CreateRegisteredBrowser(/*incognito=*/false);
  web::FakeWebState* web_state =
      AppendWebState(browser.browser.get(), /*is_visible=*/true);

  IOSChromePasskeyClient client(web_state);
  sync_pb::WebauthnCredentialSpecifics passkey;
  client.ShowPasskeyCreationInfobar(passkey);

  EXPECT_EQ(0u, InfoBarManagerImpl::FromWebState(web_state)->infobars().size());
}

// Tests that `ShowPasskeyCreationInfobar` does not add an infobar when
// `web_state_` is not visible.
TEST_F(IOSChromePasskeyClientTest,
       ShowPasskeyCreationInfobarHiddenWebStateNoOp) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_.get()), "user@example.com",
      signin::ConsentLevel::kSignin);

  RegisteredBrowser browser = CreateRegisteredBrowser(/*incognito=*/false);
  web::FakeWebState* hidden_web_state =
      AppendWebState(browser.browser.get(), /*is_visible=*/false);

  IOSChromePasskeyClient client(hidden_web_state);
  sync_pb::WebauthnCredentialSpecifics passkey;
  client.ShowPasskeyCreationInfobar(passkey);

  EXPECT_EQ(
      0u,
      InfoBarManagerImpl::FromWebState(hidden_web_state)->infobars().size());
}

// Tests that `ShowPasskeyCreationInfobar` safely no-ops if `web_state_` is
// destroyed while async passkey creation is in flight.
TEST_F(IOSChromePasskeyClientTest,
       ShowPasskeyCreationInfobarDestroyedWebStateNoOp) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_.get()), "user@example.com",
      signin::ConsentLevel::kSignin);

  RegisteredBrowser browser = CreateRegisteredBrowser(/*incognito=*/false);
  web::FakeWebState* web_state =
      AppendWebState(browser.browser.get(), /*is_visible=*/true);
  IOSChromePasskeyClient client(web_state);

  browser.browser->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);

  sync_pb::WebauthnCredentialSpecifics passkey;
  client.ShowPasskeyCreationInfobar(passkey);
}
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)

}  // namespace
