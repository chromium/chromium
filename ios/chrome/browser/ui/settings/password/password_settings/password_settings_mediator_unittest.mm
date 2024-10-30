// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/features.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/webauthn/core/browser/passkey_sync_bridge.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using ::password_manager::PasswordForm;
using ::password_manager::SavedPasswordsPresenter;
using ::password_manager::TestPasswordStore;
using ::testing::_;
using ::testing::Return;
using ::testing::WithArg;

using SyncServiceForPasswordTests = testing::NiceMock<syncer::MockSyncService>;

const trusted_vault::SecurityDomainId kPasskeysDomain =
    trusted_vault::SecurityDomainId::kPasskeys;

class MockTrustedVaultClientBackend : public TrustedVaultClientBackend {
 public:
  MockTrustedVaultClientBackend() = default;
  ~MockTrustedVaultClientBackend() override = default;

  MOCK_METHOD(void,
              SetDeviceRegistrationPublicKeyVerifierForUMA,
              (VerifierCallback verifier),
              (override));
  MOCK_METHOD(void,
              FetchKeys,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               KeyFetchedCallback completion),
              (override));
  MOCK_METHOD(void,
              MarkLocalKeysAsStale,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceClosure completion),
              (override));
  MOCK_METHOD(void,
              GetDegradedRecoverabilityStatus,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceCallback<void(bool)> completion),
              (override));
  MOCK_METHOD(CancelDialogCallback,
              Reauthentication,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               UIViewController* presenting_view_controller,
               CompletionBlock completion),
              (override));
  MOCK_METHOD(CancelDialogCallback,
              FixDegradedRecoverability,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               UIViewController* presenting_view_controller,
               CompletionBlock completion),
              (override));
  MOCK_METHOD(void,
              ClearLocalData,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceCallback<void(bool)> completion),
              (override));
  MOCK_METHOD(void,
              GetPublicKeyForIdentity,
              (id<SystemIdentity> identity, GetPublicKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateGPMPinForAccount,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               UINavigationController* navigationController,
               UIView* brandedNavigationItemTitleView,
               UpdateGPMPinCompletionCallback completion),
              (override));
};

// Sets up the provided mock sync service to respond with the given passphrase
// type and transport active/disabled state.
void SetSyncStatus(SyncServiceForPasswordTests* sync_service,
                   syncer::PassphraseType passphrase_type,
                   bool sync_transport_active = true) {
  ON_CALL(*sync_service, GetTransportState())
      .WillByDefault(
          Return(sync_transport_active
                     ? syncer::SyncService::TransportState::ACTIVE
                     : syncer::SyncService::TransportState::DISABLED));
  ON_CALL(*sync_service, GetActiveDataTypes())
      .WillByDefault(Return(syncer::DataTypeSet({syncer::PASSWORDS})));
  ON_CALL(*(sync_service->GetMockUserSettings()), GetAllEncryptedDataTypes())
      .WillByDefault(Return(syncer::DataTypeSet({syncer::PASSWORDS})));
  ON_CALL(*(sync_service->GetMockUserSettings()), GetPassphraseType())
      .WillByDefault(Return(passphrase_type));
}

}  // namespace

class PasswordSettingsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSPasskeyModelFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    profile_ = std::move(builder).Build();

    passkey_model_ = static_cast<webauthn::TestPasskeyModel*>(
        IOSPasskeyModelFactory::GetForProfile(profile_.get()));
    profile_store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    presenter_ = std::make_unique<SavedPasswordsPresenter>(
        &affiliation_service_, profile_store_, /*account_store=*/nullptr);
    trusted_vault_backend_ = std::make_unique<MockTrustedVaultClientBackend>();
  }

  void TearDown() override { [mediator_ disconnect]; }

  void CreateMediator() {
    mediator_ = [[PasswordSettingsMediator alloc]
           initWithReauthenticationModule:reauth_module_
                  savedPasswordsPresenter:presenter_.get()
        bulkMovePasswordsToAccountHandler:
            bulk_move_passwords_to_account_handler_
                            exportHandler:export_handler_
                              prefService:profile_->GetPrefs()
                          identityManager:IdentityManagerFactory::GetForProfile(
                                              profile_.get())
                              syncService:&sync_service_
                trustedVaultClientBackend:trusted_vault_backend_.get()
                                 identity:fake_identity_];
    mediator_.consumer = consumer_;
  }

  void AddPassword(std::string url,
                   std::u16string password,
                   PasswordForm::Store store) {
    auto form = std::make_unique<PasswordForm>();
    form->username_value = u"user@gmail.com";
    form->password_value = password;
    form->url = GURL(url);
    form->signon_realm = "https://www.example.com/";
    form->in_store = store;

    base::RunLoop run_loop;
    profile_store_->AddLogin(*form, run_loop.QuitClosure());
    run_loop.Run();
  }

  void AddPasskey() {
    sync_pb::WebauthnCredentialSpecifics passkey;
    passkey.set_sync_id(base::RandBytesAsString(16));
    passkey.set_credential_id(base::RandBytesAsString(16));
    passkey.set_rp_id("abc1.com");
    passkey.set_user_id({1, 2, 3, 4});
    passkey.set_user_name("passkey_username");
    passkey.set_user_display_name("passkey_display_name");

    passkey_model_->AddNewPasskeyForTesting(passkey);
  }

  web::WebTaskEnvironment task_env_;
  SyncServiceForPasswordTests sync_service_;
  affiliations::FakeAffiliationService affiliation_service_;
  raw_ptr<webauthn::TestPasskeyModel> passkey_model_;
  scoped_refptr<TestPasswordStore> profile_store_;
  std::unique_ptr<SavedPasswordsPresenter> presenter_;
  std::unique_ptr<TestProfileIOS> profile_;
  id consumer_ = OCMProtocolMock(@protocol(PasswordSettingsConsumer));
  id export_handler_ = OCMProtocolMock(@protocol(PasswordExportHandler));
  id bulk_move_passwords_to_account_handler_ =
      OCMProtocolMock(@protocol(BulkMoveLocalPasswordsToAccountHandler));
  id reauth_module_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));
  PasswordSettingsMediator* mediator_;
  std::unique_ptr<MockTrustedVaultClientBackend> trusted_vault_backend_;
  FakeSystemIdentity* fake_identity_ = [FakeSystemIdentity fakeIdentity1];
};

TEST_F(PasswordSettingsMediatorTest,
       SyncChangeTriggersChangeOnDeviceEncryption) {
  CreateMediator();

  // This was populated when the consumer was initially set.
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateNotShown];

  ASSERT_TRUE(
      [mediator_ conformsToProtocol:@protocol(SyncObserverModelBridge)]);
  PasswordSettingsMediator<SyncObserverModelBridge>* syncObserver =
      static_cast<PasswordSettingsMediator<SyncObserverModelBridge>*>(
          mediator_);

  // Set the passphrase type to "Keystore" indicating that we are not yet
  // using on-device encryption.
  SetSyncStatus(&sync_service_, syncer::PassphraseType::kKeystorePassphrase);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateOfferOptIn];

  // Set the passphrase type to "Trusted Vault" meaning on-device encryption is
  // already enabled.
  SetSyncStatus(&sync_service_,
                syncer::PassphraseType::kTrustedVaultPassphrase);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateOptedIn];

  // Turn sync transport off.
  SetSyncStatus(&sync_service_, syncer::PassphraseType::kKeystorePassphrase,
                /*sync_transport_active=*/false);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateNotShown];
}

TEST_F(PasswordSettingsMediatorTest,
       IdentityChangeTriggersChangeOnDeviceEncryption) {
  CreateMediator();
  ASSERT_TRUE(
      [mediator_ conformsToProtocol:@protocol(SyncObserverModelBridge)]);
  PasswordSettingsMediator<IdentityManagerObserverBridgeDelegate>*
      syncObserver = static_cast<
          PasswordSettingsMediator<IdentityManagerObserverBridgeDelegate>*>(
          mediator_);
  const signin::PrimaryAccountChangeEvent event;
  [syncObserver onPrimaryAccountChanged:event];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateNotShown];
}

// Tests that sync state changes trigger updates to showing the move local
// passwords to account module.
TEST_F(PasswordSettingsMediatorTest,
       SyncChangeTriggersBulkMovePasswordsToAccountChange) {
  CreateMediator();
  ASSERT_TRUE(
      [mediator_ conformsToProtocol:@protocol(SyncObserverModelBridge)]);

  PasswordSettingsMediator<SyncObserverModelBridge>* syncObserver =
      static_cast<PasswordSettingsMediator<SyncObserverModelBridge>*>(
          mediator_);

  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setLocalPasswordsCount:0 withUserEligibility:NO];
}

// Tests that update GPM Pin button is shown for a user that has GPM Pin
// created (has non-degraded recoverability status) and with a bootstrapped
// device (keys being returned from the passkey trusted vault).
TEST_F(PasswordSettingsMediatorTest, ShowsUpdateGPMPinButtonForEligibleUser) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }
  EXPECT_CALL(*trusted_vault_backend_, GetDegradedRecoverabilityStatus(
                                           fake_identity_, kPasskeysDomain, _))
      .WillOnce(WithArg<2>(
          [](auto callback) { std::move(callback).Run(/*status=*/false); }));
  EXPECT_CALL(*trusted_vault_backend_,
              FetchKeys(fake_identity_, kPasskeysDomain, _))
      .WillOnce(WithArg<2>([](auto callback) {
        std::move(callback).Run(/*shared_keys=*/{{1, 2, 3}});
      }));

  CreateMediator();
  [[consumer_ verify] setupChangeGPMPinButton];
}

// Tests that update GPM Pin button is not shown for a user that does not have
// GPM Pin created (is in degraded recoverability).
TEST_F(PasswordSettingsMediatorTest,
       DoesNotShowChangeGPMPinButtonWithNoGPMPinCreated) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }
  EXPECT_CALL(*trusted_vault_backend_, GetDegradedRecoverabilityStatus(
                                           fake_identity_, kPasskeysDomain, _))
      .WillOnce(WithArg<2>(
          [](auto callback) { std::move(callback).Run(/*status=*/true); }));

  CreateMediator();
  [[consumer_ reject] setupChangeGPMPinButton];
}

// Tests that update GPM Pin button is not shown for a user that has not
// bootstrapped their device (no keys returned from the passkey trusted vault).
TEST_F(PasswordSettingsMediatorTest,
       DoesNotShowChangeGPMPinButtonWhenNotBootstrapped) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }
  EXPECT_CALL(*trusted_vault_backend_, GetDegradedRecoverabilityStatus(
                                           fake_identity_, kPasskeysDomain, _))
      .WillOnce(WithArg<2>(
          [](auto callback) { std::move(callback).Run(/*status=*/false); }));
  EXPECT_CALL(*trusted_vault_backend_,
              FetchKeys(fake_identity_, kPasskeysDomain, _))
      .WillOnce(WithArg<2>(
          [](auto callback) { std::move(callback).Run(/*shared_keys=*/{}); }));

  CreateMediator();
  [[consumer_ reject] setupChangeGPMPinButton];
}

// Tests that passwords already in account store and passkeys are not counted
// towards the local passwords count.
TEST_F(PasswordSettingsMediatorTest, CountsProfileStorePasswordsAsLocal) {
  CreateMediator();

  AddPassword("https://www.example.com/1", u"password1",
              PasswordForm::Store::kNotSet);
  [[consumer_ verify] setLocalPasswordsCount:1 withUserEligibility:NO];
  AddPassword("https://www.example.com/2", u"password2",
              PasswordForm::Store::kProfileStore);
  [[consumer_ verify] setLocalPasswordsCount:2 withUserEligibility:NO];

  // Count should not be increased for an account store password.
  AddPassword("https://www.example.com/3", u"password3",
              PasswordForm::Store::kAccountStore);
  [[consumer_ verify] setLocalPasswordsCount:2 withUserEligibility:NO];

  if (syncer::IsWebauthnCredentialSyncEnabled()) {
    // Count should not be increased for a passkey.
    AddPasskey();
    [[consumer_ verify] setLocalPasswordsCount:2 withUserEligibility:NO];
  }
}
