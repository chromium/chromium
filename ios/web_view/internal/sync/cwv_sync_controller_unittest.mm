// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using testing::_;
using testing::Invoke;
using testing::Return;

class CWVSyncControllerTest : public PlatformTest {
 protected:
  CWVSyncControllerTest()
      : browser_state_(/*off_the_record=*/false),
        signin_client_(browser_state_.GetPrefs()),
        sigin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT),
        token_service_(
            browser_state_.GetPrefs(),
            std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
                &signin_client_,
                std::make_unique<FakeProfileOAuth2TokenServiceIOSProvider>(),
                &account_tracker_service_,
                &sigin_error_controller_)),
        gaia_cookie_manager_service_(&token_service_,
                                     "cookie-source",
                                     &signin_client_),
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_service_,
                        &gaia_cookie_manager_service_) {
    web_state_.SetBrowserState(&browser_state_);

    browser_sync::ProfileSyncService::InitParams init_params;
    init_params.start_behavior = browser_sync::ProfileSyncService::MANUAL_START;
    init_params.sync_client = std::make_unique<syncer::FakeSyncClient>();
    init_params.url_loader_factory = browser_state_.GetSharedURLLoaderFactory();
    init_params.network_time_update_callback = base::DoNothing();
    init_params.signin_scoped_device_id_callback = base::BindRepeating(
        &signin::GetSigninScopedDeviceId, browser_state_.GetPrefs());
    profile_sync_service_ =
        std::make_unique<browser_sync::ProfileSyncServiceMock>(&init_params);

    account_tracker_service_.Initialize(browser_state_.GetPrefs(),
                                        base::FilePath());

    EXPECT_CALL(*profile_sync_service_, AddObserver(_))
        .WillOnce(Invoke(this, &CWVSyncControllerTest::AddObserver));

    sync_controller_ = [[CWVSyncController alloc]
        initWithProfileSyncService:profile_sync_service_.get()
             accountTrackerService:&account_tracker_service_
                     signinManager:&signin_manager_
                      tokenService:&token_service_];
  };

  ~CWVSyncControllerTest() override {
    EXPECT_CALL(*profile_sync_service_, RemoveObserver(_));
  }

  void AddObserver(syncer::SyncServiceObserver* observer) {
    sync_service_observer_ = observer;
  }

  void OnConfigureDone(const syncer::DataTypeManager::ConfigureResult& result) {
    sync_service_observer_->OnSyncConfigurationCompleted(
        profile_sync_service_.get());
  }

  web::TestWebThreadBundle web_thread_bundle_;
  ios_web_view::WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  std::unique_ptr<browser_sync::ProfileSyncServiceMock> profile_sync_service_;
  AccountTrackerService account_tracker_service_;
  TestSigninClient signin_client_;
  SigninErrorController sigin_error_controller_;
  FakeProfileOAuth2TokenService token_service_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;
  FakeSigninManager signin_manager_;
  CWVSyncController* sync_controller_;
  syncer::SyncServiceObserver* sync_service_observer_;
};

// Verifies CWVSyncControllerDataSource methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DataSourceCallbacks) {
  // [data_source expect] returns an autoreleased object, but it must be
  // destroyed before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));

    [[data_source expect]
                 syncController:sync_controller_
        getAccessTokenForScopes:[OCMArg checkWithBlock:^BOOL(NSArray* scopes) {
          return [scopes containsObject:@"scope1.chromium.org"] &&
                 [scopes containsObject:@"scope2.chromium.org"];
        }]
              completionHandler:[OCMArg any]];

    EXPECT_CALL(*profile_sync_service_, RequestStart());
    EXPECT_CALL(*profile_sync_service_, SetFirstSetupComplete());

    CWVIdentity* identity =
        [[CWVIdentity alloc] initWithEmail:@"johndoe@chromium.org"
                                  fullName:@"John Doe"
                                    gaiaID:@"1337"];
    [sync_controller_ startSyncWithIdentity:identity dataSource:data_source];

    std::set<std::string> scopes = {"scope1.chromium.org",
                                    "scope2.chromium.org"};
    ProfileOAuth2TokenServiceIOSProvider::AccessTokenCallback callback;
    [sync_controller_ fetchAccessTokenForScopes:scopes callback:callback];

    [data_source verify];
  }
}

// Verifies CWVSyncControllerDelegate methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DelegateCallbacks) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVSyncControllerDelegate));
    sync_controller_.delegate = delegate;

    [[delegate expect] syncControllerDidStartSync:sync_controller_];
    EXPECT_CALL(*profile_sync_service_, OnConfigureDone(_))
        .WillOnce(Invoke(
            this,
            &CWVSyncControllerTest_DelegateCallbacks_Test::OnConfigureDone));
    syncer::DataTypeManager::ConfigureResult result;
    profile_sync_service_->OnConfigureDone(result);

    [[delegate expect]
          syncController:sync_controller_
        didFailWithError:[OCMArg checkWithBlock:^BOOL(NSError* error) {
          return error.code == CWVSyncErrorInvalidGAIACredentials;
        }]];
    GoogleServiceAuthError auth_error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    [sync_controller_ didUpdateAuthError:auth_error];

    [[delegate expect] syncController:sync_controller_
                didStopSyncWithReason:CWVStopSyncReasonServer];
    [sync_controller_
        didSignoutWithSourceMetric:signin_metrics::ProfileSignout::
                                       SERVER_FORCED_DISABLE];

    [delegate verify];
  }
}

// Verifies CWVSyncController properly maintains the current syncing user.
TEST_F(CWVSyncControllerTest, CurrentIdentity) {
  EXPECT_CALL(*profile_sync_service_, RequestStart());
  EXPECT_CALL(*profile_sync_service_, SetFirstSetupComplete());
  CWVIdentity* identity =
      [[CWVIdentity alloc] initWithEmail:@"johndoe@chromium.org"
                                fullName:@"John Doe"
                                  gaiaID:@"1337"];
  id unused_mock = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
  [sync_controller_ startSyncWithIdentity:identity dataSource:unused_mock];
  CWVIdentity* currentIdentity = sync_controller_.currentIdentity;
  EXPECT_TRUE(currentIdentity);
  EXPECT_NSEQ(identity.email, currentIdentity.email);
  EXPECT_NSEQ(identity.fullName, currentIdentity.fullName);
  EXPECT_NSEQ(identity.gaiaID, currentIdentity.gaiaID);

  EXPECT_CALL(*profile_sync_service_, RequestStop(_));
  [sync_controller_ stopSyncAndClearIdentity];
  EXPECT_FALSE(sync_controller_.currentIdentity);
}

// Verifies CWVSyncController's passphrase API.
TEST_F(CWVSyncControllerTest, Passphrase) {
  EXPECT_CALL(*profile_sync_service_, IsPassphraseRequiredForDecryption())
      .WillOnce(Return(true));
  EXPECT_TRUE(sync_controller_.passphraseNeeded);
  EXPECT_CALL(*profile_sync_service_,
              SetDecryptionPassphrase("dummy-passphrase"))
      .WillOnce(Return(true));
  EXPECT_TRUE([sync_controller_ unlockWithPassphrase:@"dummy-passphrase"]);
}

}  // namespace ios_web_view
