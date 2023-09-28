// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <memory>
#import <set>

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/test_signin_client.h"
#import "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/identity_manager_builder.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/service/sync_service_observer.h"
#import "components/sync/test/test_sync_service.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace ios_web_view {
namespace {

const char kTestEmail[] = "johndoe@chromium.org";

}  // namespace

class CWVSyncControllerTest : public PlatformTest {
 protected:
  CWVSyncControllerTest() {
    pref_service_.registry()->RegisterDictionaryPref(
        autofill::prefs::kAutofillSyncTransportOptIn);

    // Change the default transport state to be disabled.
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::DISABLED);
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestSyncService sync_service_;
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(CWVSyncControllerTest, StartSyncWithIdentity) {
  CoreAccountInfo account_info =
      identity_test_environment_.MakeAccountAvailable(kTestEmail);

  CWVIdentity* identity = [[CWVIdentity alloc]
      initWithEmail:@(kTestEmail)
           fullName:nil
             gaiaID:base::SysUTF8ToNSString(account_info.gaia)];

  // Preconfigure TestSyncService as if it was enabled in transport mode.
  sync_service_.SetInitialSyncFeatureSetupComplete(false);
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.SetIsUsingExplicitPassphrase(false);
  sync_service_.SetAccountInfo(account_info);

  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];
  [sync_controller startSyncWithIdentity:identity];
  EXPECT_NSEQ(sync_controller.currentIdentity.gaiaID, identity.gaiaID);

  CoreAccountInfo primary_account_info =
      identity_test_environment_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  EXPECT_EQ(primary_account_info, account_info);

  // Ensure opt-ins for transport only sync data is flipped to true.
  EXPECT_TRUE(autofill::prefs::IsUserOptedInWalletSyncTransport(
      &pref_service_, primary_account_info.account_id));
  EXPECT_EQ(password_manager::features_util::GetDefaultPasswordStore(
                &pref_service_, &sync_service_),
            password_manager::PasswordForm::Store::kAccountStore);
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      &pref_service_, &sync_service_));
}

TEST_F(CWVSyncControllerTest, StopSyncAndClearIdentity) {
  CoreAccountInfo account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];
  CWVIdentity* current_identity = sync_controller.currentIdentity;
  ASSERT_TRUE(current_identity);
  EXPECT_NSEQ(current_identity.gaiaID,
              base::SysUTF8ToNSString(account_info.gaia));
  EXPECT_NSEQ(current_identity.email, base::SysUTF8ToNSString(kTestEmail));

  [sync_controller stopSyncAndClearIdentity];
  EXPECT_FALSE(sync_controller.currentIdentity);
}

TEST_F(CWVSyncControllerTest, PassphraseNeeded) {
  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];
  sync_service_.SetPassphraseRequiredForPreferredDataTypes(false);
  EXPECT_FALSE(sync_controller.passphraseNeeded);
  sync_service_.SetPassphraseRequiredForPreferredDataTypes(true);
  EXPECT_TRUE(sync_controller.passphraseNeeded);
}

TEST_F(CWVSyncControllerTest, TrustedVaultKeysRequired) {
  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];
  sync_service_.SetTrustedVaultKeyRequiredForPreferredDataTypes(false);
  EXPECT_FALSE(sync_controller.trustedVaultKeysRequired);
  sync_service_.SetTrustedVaultKeyRequiredForPreferredDataTypes(true);
  EXPECT_TRUE(sync_controller.trustedVaultKeysRequired);
}

TEST_F(CWVSyncControllerTest, TrustedVaultRecoverabilityDegraded) {
  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];
  sync_service_.SetTrustedVaultRecoverabilityDegraded(false);
  EXPECT_FALSE(sync_controller.trustedVaultRecoverabilityDegraded);
  sync_service_.SetTrustedVaultRecoverabilityDegraded(true);
  EXPECT_TRUE(sync_controller.trustedVaultRecoverabilityDegraded);
}

TEST_F(CWVSyncControllerTest, DelegateDidStartAndStopSync) {
  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];

  id delegate = OCMStrictProtocolMock(@protocol(CWVSyncControllerDelegate));
  [delegate setExpectationOrderMatters:YES];
  sync_controller.delegate = delegate;

  // TestSyncService's transport state has to actually change before a callback
  // will be fired, so we have to start it before we can stop it.
  OCMExpect([delegate syncControllerDidStartSync:sync_controller]);
  OCMExpect([delegate syncControllerDidUpdateState:sync_controller]);
  OCMExpect([delegate syncControllerDidStopSync:sync_controller]);
  OCMExpect([delegate syncControllerDidUpdateState:sync_controller]);
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.FireStateChanged();
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  sync_service_.FireStateChanged();

  [delegate verify];
}

TEST_F(CWVSyncControllerTest, DelegateDidUpdateState) {
  CWVSyncController* sync_controller = [[CWVSyncController alloc]
      initWithSyncService:&sync_service_
          identityManager:identity_test_environment_.identity_manager()
              prefService:&pref_service_];

  id delegate = OCMStrictProtocolMock(@protocol(CWVSyncControllerDelegate));
  sync_controller.delegate = delegate;

  OCMExpect([delegate syncControllerDidUpdateState:sync_controller]);
  sync_service_.FireStateChanged();
  [delegate verify];
}

}  // namespace ios_web_view
