// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "components/sync/base/model_type.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::SavedPasswordsPresenter;
using password_manager::TestPasswordStore;

using SyncServiceForPasswordTests = testing::NiceMock<syncer::MockSyncService>;

// Sets up the provided mock sync service to respond with the given passphrase
// type and transport active/disabled state.
void SetSyncStatus(SyncServiceForPasswordTests* sync_service,
                   syncer::PassphraseType passphrase_type,
                   bool sync_transport_active = true) {
  ON_CALL(*sync_service, GetTransportState())
      .WillByDefault(
          testing::Return(sync_transport_active
                              ? syncer::SyncService::TransportState::ACTIVE
                              : syncer::SyncService::TransportState::DISABLED));
  ON_CALL(*sync_service, GetActiveDataTypes())
      .WillByDefault(testing::Return(syncer::ModelTypeSet(syncer::PASSWORDS)));
  ON_CALL(*(sync_service->GetMockUserSettings()), GetEncryptedDataTypes())
      .WillByDefault(testing::Return(syncer::ModelTypeSet(syncer::PASSWORDS)));
  ON_CALL(*(sync_service->GetMockUserSettings()), GetPassphraseType())
      .WillByDefault(testing::Return(passphrase_type));
}

// Sets up a password store factory for testing, and returns the test store.
scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    ChromeBrowserState* browser_state) {
  return base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
      IOSChromePasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              browser_state,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  web::BrowserState, TestPasswordStore>))
          .get()));
}

class PasswordSettingsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();

    password_manager::MockAffiliationService affiliation_service_;
    store_ = CreateAndUseTestPasswordStore(browser_state_.get());
    presenter_ = std::make_unique<SavedPasswordsPresenter>(
        &affiliation_service_, store_, /*accont_store=*/nullptr);

    mediator_ = [[PasswordSettingsMediator alloc]
        initWithReauthenticationModule:reauth_module_
               savedPasswordsPresenter:presenter_.get()
                         exportHandler:export_handler_
                           prefService:browser_state_->GetPrefs()
                       identityManager:IdentityManagerFactory::
                                           GetForBrowserState(
                                               browser_state_.get())
                           syncService:&sync_service_];
    mediator_.consumer = consumer_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  web::WebTaskEnvironment task_env_;
  SyncServiceForPasswordTests sync_service_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<SavedPasswordsPresenter> presenter_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  id consumer_ = OCMProtocolMock(@protocol(PasswordSettingsConsumer));
  id export_handler_ = OCMProtocolMock(@protocol(PasswordExportHandler));
  id reauth_module_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));
  PasswordSettingsMediator* mediator_;
};

TEST_F(PasswordSettingsMediatorTest,
       SyncChangeTriggersChangeOnDeviceEncryption) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {syncer::kSyncTrustedVaultPassphraseRecovery,
       syncer::kSyncTrustedVaultPassphrasePromo},
      {});

  // This was populated when the consumer was initially set.
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateNotShown];

  ASSERT_TRUE(
      [mediator_ conformsToProtocol:@protocol(SyncObserverModelBridge)]);
  PasswordSettingsMediator<SyncObserverModelBridge>* syncObserver =
      static_cast<PasswordSettingsMediator<SyncObserverModelBridge>*>(
          mediator_);

  // Set the passphrase type to "Keystore," indicating that we are not yet
  // using on-device encryption.
  SetSyncStatus(&sync_service_, syncer::PassphraseType::kKeystorePassphrase);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateOfferOptIn];

  // Set the passphrase type to "Trusted Vault," meaning on-device encryption is
  // already enabled.
  SetSyncStatus(&sync_service_,
                syncer::PassphraseType::kTrustedVaultPassphrase);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateOptedIn];

  // Turn sync trasnport off.
  SetSyncStatus(&sync_service_, syncer::PassphraseType::kKeystorePassphrase,
                /* sync_transport_active= */ false);
  [syncObserver onSyncStateChanged];
  [[consumer_ verify] setOnDeviceEncryptionState:
                          PasswordSettingsOnDeviceEncryptionStateNotShown];
}

TEST_F(PasswordSettingsMediatorTest,
       IdentityChangeTriggersChangeOnDeviceEncryption) {
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
