// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/web_client.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"
#include "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace {

using testing::_;
using testing::Invoke;
using testing::Return;

const char kTestGaiaId[] = "1337";
const char kTestEmail[] = "johndoe@chromium.org";
const char kTestFullName[] = "John Doe";
const char kTestPassphrase[] = "dummy-passphrase";
const char kTestScope1[] = "scope1.chromium.org";
const char kTestScope2[] = "scope2.chromium.org";

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

}  // namespace

class CWVSyncControllerTest : public TestWithLocaleAndResources {
 protected:
  CWVSyncControllerTest()
      : web_client_(std::make_unique<web::WebClient>()),
        browser_state_(/*off_the_record=*/false) {
    // Clear account info before each test.
    PrefService* pref_service = browser_state_.GetPrefs();
    pref_service->ClearPref(prefs::kGoogleServicesAccountId);
    pref_service->ClearPref(prefs::kGoogleServicesConsentedToSync);
    pref_service->ClearPref(prefs::kAccountInfo);

    WebViewProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        &browser_state_, base::BindRepeating(&BuildMockSyncService));

    EXPECT_CALL(*mock_sync_service(), AddObserver(_))
        .WillOnce(Invoke(this, &CWVSyncControllerTest::AddObserver));

    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();

    password_store_ = new password_manager::TestPasswordStore();
    password_store_->Init(base::RepeatingCallback<void(syncer::ModelType)>(),
                          nullptr);

    sync_controller_ = [[CWVSyncController alloc]
          initWithSyncService:mock_sync_service()
              identityManager:identity_manager()
        signinErrorController:signin_error_controller()
          personalDataManager:personal_data_manager_.get()
                passwordStore:password_store_.get()];
  }

  ~CWVSyncControllerTest() override {
    password_store_->ShutdownOnUIThread();
    EXPECT_CALL(*mock_sync_service(), RemoveObserver(_));
    EXPECT_CALL(*mock_sync_service(), Shutdown());
  }

  void AddObserver(syncer::SyncServiceObserver* observer) {
    sync_service_observer_ = observer;
  }

  signin::IdentityManager* identity_manager() {
    return WebViewIdentityManagerFactory::GetForBrowserState(&browser_state_);
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        WebViewProfileSyncServiceFactory::GetForBrowserState(&browser_state_));
  }

  SigninErrorController* signin_error_controller() {
    return WebViewSigninErrorControllerFactory::GetForBrowserState(
        &browser_state_);
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  ios_web_view::WebViewBrowserState browser_state_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  CWVSyncController* sync_controller_ = nil;
  syncer::SyncServiceObserver* sync_service_observer_ = nullptr;
};

// Verifies CWVSyncControllerDataSource methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DataSourceCallbacks) {
  // [data_source expect] returns an autoreleased object, but it must be
  // destroyed before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
    CWVSyncController.dataSource = data_source;
    [[data_source expect]
        fetchAccessTokenForIdentity:[OCMArg checkWithBlock:^BOOL(
                                                CWVIdentity* identity) {
          return [identity.gaiaID isEqualToString:@(kTestGaiaId)];
        }]
                             scopes:[OCMArg checkWithBlock:^BOOL(
                                                NSArray* scopes) {
                               return [scopes containsObject:@(kTestScope1)] &&
                                      [scopes containsObject:@(kTestScope2)];
                             }]
                  completionHandler:[OCMArg any]];

    WebViewDeviceAccountsProviderImpl accounts_provider;
    std::set<std::string> scopes = {kTestScope1, kTestScope2};
    accounts_provider.GetAccessToken(kTestGaiaId, "dummy-client-id", scopes,
                                     base::DoNothing());

    [data_source verify];
  }
}

// Verifies CWVSyncControllerDelegate methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DelegateCallbacks) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@(kTestEmail)
                                                      fullName:@(kTestFullName)
                                                        gaiaID:@(kTestGaiaId)];
    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
    [[[data_source stub] andReturn:@[ identity ]] allKnownIdentities];
    CWVSyncController.dataSource = data_source;
    id delegate = OCMProtocolMock(@protocol(CWVSyncControllerDelegate));
    sync_controller_.delegate = delegate;
    [sync_controller_ startSyncWithIdentity:identity];

    [[delegate expect] syncControllerDidStartSync:sync_controller_];
    sync_service_observer_->OnSyncConfigurationCompleted(mock_sync_service());
    [[delegate expect]
          syncController:sync_controller_
        didFailWithError:[OCMArg checkWithBlock:^BOOL(NSError* error) {
          return error.code == CWVSyncErrorInvalidGAIACredentials;
        }]];

    // Create authentication error.
    GoogleServiceAuthError auth_error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager(), identity_manager()->GetPrimaryAccountId(),
        auth_error);

    [[delegate expect] syncControllerDidStopSync:sync_controller_];
    identity_manager()->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
        signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
        signin_metrics::SignoutDelete::IGNORE_METRIC);

    [delegate verify];
  }
}

// Verifies CWVSyncController properly maintains the current syncing user.
TEST_F(CWVSyncControllerTest, CurrentIdentity) {
  CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@(kTestEmail)
                                                    fullName:@(kTestFullName)
                                                      gaiaID:@(kTestGaiaId)];
  id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
  [[[data_source stub] andReturn:@[ identity ]] allKnownIdentities];
  CWVSyncController.dataSource = data_source;
  [sync_controller_ startSyncWithIdentity:identity];
  CWVIdentity* currentIdentity = sync_controller_.currentIdentity;
  EXPECT_TRUE(currentIdentity);
  EXPECT_NSEQ(identity.email, currentIdentity.email);
  EXPECT_NSEQ(identity.fullName, currentIdentity.fullName);
  EXPECT_NSEQ(identity.gaiaID, currentIdentity.gaiaID);

  EXPECT_CALL(*mock_sync_service(), StopAndClear());
  [sync_controller_ stopSyncAndClearIdentity];
  EXPECT_FALSE(sync_controller_.currentIdentity);
}

// Verifies CWVSyncController's passphrase API.
TEST_F(CWVSyncControllerTest, Passphrase) {
  EXPECT_CALL(*mock_sync_service()->GetMockUserSettings(),
              IsPassphraseRequiredForPreferredDataTypes())
      .WillOnce(Return(true));
  EXPECT_TRUE(sync_controller_.passphraseNeeded);
  EXPECT_CALL(*mock_sync_service()->GetMockUserSettings(),
              SetDecryptionPassphrase(kTestPassphrase))
      .WillOnce(Return(true));
  EXPECT_TRUE([sync_controller_ unlockWithPassphrase:@(kTestPassphrase)]);
}

}  // namespace ios_web_view
