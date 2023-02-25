// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager_builder.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service_observer.h"
#import "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace {

const char kTestEmail[] = "johndoe@chromium.org";

}  // namespace

class CWVSyncControllerTest : public PlatformTest {
 protected:
  CWVSyncControllerTest() {
    scoped_feature_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);

    pref_service_.registry()->RegisterDictionaryPref(
        autofill::prefs::kAutofillSyncTransportOptIn);
    pref_service_.registry()->RegisterDictionaryPref(
        password_manager::prefs::kAccountStoragePerAccountSettings);

    // Change the default transport state to be disabled.
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::DISABLED);
  }

  base::test::ScopedFeatureList scoped_feature_;
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
  sync_service_.SetFirstSetupComplete(false);
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
          signin::ConsentLevel::kSync);
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
          kTestEmail, signin::ConsentLevel::kSync);

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
