// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_mediator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/signin/core/browser/test_account_preview_data_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/metrics/model/ios_profile_metrics_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using AccountPreviewPreference =
    signin::AccountPreviewDataService::AccountPreviewPreference;

// Fake consumer which records the email of the selected identity and whether
// UI interaction is enabled.
@interface FakeFullscreenSigninScreenConsumer
    : NSObject <FullscreenSigninScreenConsumer>
@property(nonatomic, assign) BOOL hasPlatformPolicies;
@property(nonatomic, assign) SigninScreenConsumerScreenIntent screenIntent;
@property(nonatomic, assign) SigninScreenConsumerSigninStatus signinStatus;
@property(nonatomic, assign) BOOL syncEnabled;
@property(nonatomic, copy) NSString* currentPrimaryIdentityEmail;
@property(nonatomic, copy) NSString* targetIdentityEmail;
@property(nonatomic, copy) NSString* selectedEmail;
@property(nonatomic, assign) BOOL UIEnabled;
@end

@implementation FakeFullscreenSigninScreenConsumer

- (instancetype)init {
  self = [super init];
  if (self) {
    _UIEnabled = YES;
  }
  return self;
}

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar
                            managed:(BOOL)managed {
  self.selectedEmail = email;
}

- (void)noIdentityAvailable {
  self.selectedEmail = nil;
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  _UIEnabled = UIEnabled;
}

@end

namespace {

AccountPreviewPreference CreatePreference(const GaiaId& gaia_id) {
  AccountPreviewPreference preference;
  preference.gaia_id = gaia_id;
  return preference;
}

}  // namespace

class FullscreenSigninScreenMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    // The profile must be registered with the profile manager so that the
    // identities on the device are attached to it.
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    // `default_identity_` is the default identity on device, as it is added
    // first.
    GetSystemIdentityManager()->AddIdentity(default_identity_);
    GetSystemIdentityManager()->AddIdentity(preferred_identity_);

    consumer_ = [[FakeFullscreenSigninScreenConsumer alloc] init];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Creates the mediator for `access_point`, and sets its consumer.
  FullscreenSigninScreenMediator* CreateMediatorWithConsumer(
      signin_metrics::AccessPoint access_point) {
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile_.get());
    metrics::ProfileMetricsService* profile_metrics_service =
        IOSProfileMetricsServiceFactory::GetForProfile(profile_.get());

    mediator_ = [[FullscreenSigninScreenMediator alloc]
            initWithAccountManagerService:account_manager_service
                    authenticationService:authentication_service
                          identityManager:identity_manager
                accountPreviewDataService:&account_preview_data_service_
                         localPrefService:GetApplicationContext()
                                              ->GetLocalState()
                              prefService:profile_->GetPrefs()
                              syncService:sync_service
                         selectedIdentity:nil
                              accessPoint:access_point
                              promoAction:signin_metrics::PromoAction::
                                              PROMO_ACTION_NO_SIGNIN_PROMO
                    profileMetricsService:profile_metrics_service
        changeProfileContinuationProvider:DoNothingContinuationProvider()];
    mediator_.consumer = consumer_;
    return mediator_;
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  signin::TestAccountPreviewDataService account_preview_data_service_;
  FakeSystemIdentity* default_identity_ = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* preferred_identity_ = [FakeSystemIdentity fakeIdentity2];
  FakeFullscreenSigninScreenConsumer* consumer_ = nil;
  FullscreenSigninScreenMediator* mediator_ = nil;
};

// Tests that the preferred account is not requested in the FRE if the FRE
// feature is disabled, even if the non-FRE feature is enabled.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotRequestPreferredAccountInFREIfFREFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kEnableAccountPreviewPreferredAccount},
      /*disabled_features=*/{
          switches::kEnableAccountPreviewPreferredAccountFRE});
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));
  account_preview_data_service_.set_defer_preferred_account_requests(true);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.UIEnabled);
}

// Tests that the FRE preferred account feature does not select the preferred
// account outside of the FRE, whereas the non-FRE feature does.
TEST_F(FullscreenSigninScreenMediatorTest,
       PreferredAccountSelectionOutsideFRE) {
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{switches::
                                  kEnableAccountPreviewPreferredAccountFRE},
        /*disabled_features=*/{
            switches::kEnableAccountPreviewPreferredAccount});

    FullscreenSigninScreenMediator* mediator = CreateMediatorWithConsumer(
        signin_metrics::AccessPoint::kFullscreenSigninPromo);
    EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
    [mediator disconnect];
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{switches::kEnableAccountPreviewPreferredAccount},
        /*disabled_features=*/{
            switches::kEnableAccountPreviewPreferredAccountFRE});

    FullscreenSigninScreenMediator* mediator = CreateMediatorWithConsumer(
        signin_metrics::AccessPoint::kFullscreenSigninPromo);
    EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  }
}

// Tests that the preferred identity is selected right away if the preferred
// account is not being computed.
TEST_F(FullscreenSigninScreenMediatorTest,
       SelectsPreferredIdentityImmediately) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(preferred_identity_.userEmail, consumer_.selectedEmail);
  EXPECT_TRUE(consumer_.UIEnabled);
}

// Tests that the default identity is selected and UI interaction is disabled
// while the preferred account is being computed, and that the preferred
// identity is selected and UI interaction re-enabled once computed before the
// timeout.
TEST_F(FullscreenSigninScreenMediatorTest,
       SelectsPreferredIdentityOnceComputed) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(default_identity_.userEmail, consumer_.selectedEmail);
  EXPECT_FALSE(consumer_.UIEnabled);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(preferred_identity_.userEmail, consumer_.selectedEmail);
  EXPECT_TRUE(consumer_.UIEnabled);
}

// Tests that the preferred account is ignored if it finishes computing after
// the timeout configured in the feature param.
TEST_F(FullscreenSigninScreenMediatorTest,
       IgnoresPreferredAccountAfterTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewPreferredAccountFRE,
      {{switches::kAccountPreviewPreferredAccountFREFetchTimeout.name, "3s"}});
  account_preview_data_service_.set_defer_preferred_account_requests(true);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_FALSE(consumer_.UIEnabled);

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(consumer_.UIEnabled);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
}

// Tests that the default identity is kept if the preferred account is not on
// the device.
TEST_F(FullscreenSigninScreenMediatorTest,
       KeepsDefaultIdentityIfPreferredAccountNotOnDevice) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(GaiaId("unknown_gaia_id")));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.UIEnabled);
}

// Tests that the preferred account is not requested when there are no accounts
// on the device.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotRequestPreferredAccountWhenNoAccountsOnDevice) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      preferred_identity_);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  EXPECT_EQ(nil, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.UIEnabled);
}

// Tests that the preferred account is ignored and UI is re-enabled once the
// mediator is disconnected.
TEST_F(FullscreenSigninScreenMediatorTest,
       IgnoresPreferredAccountAfterDisconnect) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_FALSE(consumer_.UIEnabled);

  [mediator disconnect];
  EXPECT_TRUE(consumer_.UIEnabled);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
}

// Tests that if the currently selected identity is removed in the FRE, the
// synchronous preferred account is selected if available.
TEST_F(FullscreenSigninScreenMediatorTest,
       SelectsPreferredIdentityWhenSelectedIdentityRemovedInFRE) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  FakeSystemIdentity* third_identity = [FakeSystemIdentity fakeIdentity3];
  GetSystemIdentityManager()->AddIdentity(third_identity);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(third_identity.gaiaId));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(third_identity, mediator.selectedIdentity);

  // User manually switches to `default_identity_`, which is then removed from
  // the device.
  mediator.selectedIdentity = default_identity_;
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);

  // `third_identity` (the preferred account) should be selected rather than
  // `preferred_identity_` (the new default identity on device).
  EXPECT_NSEQ(third_identity, mediator.selectedIdentity);
}

// Tests that waiting for the preferred account stops, UI is re-enabled, and
// the pending callback is cancelled if all accounts on the device are removed
// while waiting.
TEST_F(FullscreenSigninScreenMediatorTest,
       StopsWaitingWhenAllAccountsRemovedInFRE) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_FALSE(consumer_.UIEnabled);

  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      preferred_identity_);

  EXPECT_EQ(nil, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.UIEnabled);

  // Re-adding identities and completing the pending request should not select
  // `preferred_identity_` because the request was cancelled.
  GetSystemIdentityManager()->AddIdentity(default_identity_);
  GetSystemIdentityManager()->AddIdentity(preferred_identity_);
  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
}
