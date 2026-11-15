// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_mediator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/test_account_preview_data_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/data_type.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync/test/test_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/metrics/model/ios_profile_metrics_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using AccountPreviewPreference =
    signin::AccountPreviewDataService::AccountPreviewPreference;

// Fake consumer which records the email of the selected identity and the
// content loading state.
@interface FakeFullscreenSigninScreenConsumer
    : NSObject <FullscreenSigninScreenConsumer>
@property(nonatomic, assign) BOOL hasPlatformPolicies;
@property(nonatomic, assign) SigninScreenConsumerScreenIntent screenIntent;
@property(nonatomic, assign) SigninScreenConsumerSigninStatus signinStatus;
@property(nonatomic, assign) BOOL syncEnabled;
@property(nonatomic, copy) NSString* currentPrimaryIdentityEmail;
@property(nonatomic, copy) NSString* targetIdentityEmail;
@property(nonatomic, copy) NSString* subtitleOverride;
@property(nonatomic, copy) NSString* subtitleOverrideWhenContentLoadingStopped;
@property(nonatomic, copy) NSString* selectedEmail;
@property(nonatomic, copy) NSString* selectedEmailWhenContentLoadingStopped;
@property(nonatomic, assign) BOOL contentLoading;
@property(nonatomic, assign) BOOL wasContentLoading;
@end

@implementation FakeFullscreenSigninScreenConsumer

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
  // No-op.
}

- (void)setContentLoading:(BOOL)contentLoading {
  if (contentLoading) {
    self.wasContentLoading = YES;
  } else if (_contentLoading) {
    self.selectedEmailWhenContentLoadingStopped = self.selectedEmail;
    self.subtitleOverrideWhenContentLoadingStopped = self.subtitleOverride;
  }
  _contentLoading = contentLoading;
}

@end

namespace {

AccountPreviewPreference CreatePreference(const GaiaId& gaia_id) {
  AccountPreviewPreference preference;
  preference.gaia_id = gaia_id;
  preference.preferred_data_types.push_back({.data_type = syncer::PASSWORDS});
  preference.other_device_info.form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
  preference.other_device_info.enabled_data_types = {syncer::PASSWORDS};
  return preference;
}

NSString* ExpectedDefaultSubtitleOverride() {
  return l10n_util::GetNSStringF(
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_PASSWORDS,
      l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_PHONE));
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
      signin_metrics::AccessPoint access_point,
      id<SystemIdentity> selected_identity = nil) {
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
                         selectedIdentity:selected_identity
                              accessPoint:access_point
                              promoAction:signin_metrics::PromoAction::
                                              PROMO_ACTION_NO_SIGNIN_PROMO
                    profileMetricsService:profile_metrics_service
        changeProfileContinuationProvider:DoNothingContinuationProvider()];
    mediator_.consumer = consumer_;
    return mediator_;
  }

  // Starts `AuthenticationFlow` on `mediator_` and completes it with `identity`
  // and `cancelation_reason`.
  void SimulateSignInWithIdentity(
      id<SystemIdentity> identity,
      signin_ui::CancelationReason cancelation_reason) {
    AuthenticationFlow* authentication_flow_mock =
        OCMStrictClassMock([AuthenticationFlow class]);
    __block id<AuthenticationFlowDelegate> authentication_flow_delegate = nil;
    __block bool completion_called = false;
    OCMExpect([authentication_flow_mock
        setDelegate:[OCMArg checkWithBlock:^BOOL(
                                id<AuthenticationFlowDelegate> value) {
          authentication_flow_delegate = value;
          return value == static_cast<id>(mediator_);
        }]]);
    OCMExpect([authentication_flow_mock startSignIn])
        .andDo(^(NSInvocation* invocation) {
          [authentication_flow_delegate
              authenticationFlowDidSignInInSameProfileWithIdentity:identity
                                                 cancelationReason:
                                                     cancelation_reason
                                                        completion:^{
                                                          completion_called =
                                                              true;
                                                        }];
        });
    [mediator_ startSignInWithAuthenticationFlow:authentication_flow_mock];
    EXPECT_TRUE(completion_called);
    EXPECT_OCMOCK_VERIFY(static_cast<id>(authentication_flow_mock));
  }

  web::WebTaskEnvironment task_environment_;
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
  EXPECT_EQ(nil, consumer_.subtitleOverride);
  EXPECT_FALSE(consumer_.contentLoading);
}

// Tests that the FRE preferred account feature does not select the preferred
// account outside of the FRE, whereas the non-FRE feature does, and that the
// subtitle override is only set when the FRE feature is also enabled.
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
    EXPECT_EQ(nil, consumer_.subtitleOverride);
    EXPECT_FALSE(consumer_.contentLoading);
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
    EXPECT_EQ(nil, consumer_.subtitleOverride);
    EXPECT_FALSE(consumer_.contentLoading);
    [mediator disconnect];
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{switches::kEnableAccountPreviewPreferredAccount,
                              switches::
                                  kEnableAccountPreviewPreferredAccountFRE},
        /*disabled_features=*/{});

    FullscreenSigninScreenMediator* mediator = CreateMediatorWithConsumer(
        signin_metrics::AccessPoint::kFullscreenSigninPromo);
    EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
    EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);
    EXPECT_FALSE(consumer_.contentLoading);
  }
}

// Tests that the preferred identity is selected asynchronously without waiting
// for a fetch if the preferred account is already available.
TEST_F(FullscreenSigninScreenMediatorTest,
       SelectsPreferredIdentityImmediately) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.contentLoading);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(preferred_identity_.userEmail, consumer_.selectedEmail);
  EXPECT_NSEQ(preferred_identity_.userEmail,
              consumer_.selectedEmailWhenContentLoadingStopped);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(),
              consumer_.subtitleOverrideWhenContentLoadingStopped);
  EXPECT_TRUE(consumer_.wasContentLoading);
  EXPECT_FALSE(consumer_.contentLoading);
}

// Tests that the default identity is selected and content loading is shown
// while the preferred account is being computed, and that the preferred
// identity is selected and content loading stopped once computed before the
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
  EXPECT_EQ(nil, consumer_.subtitleOverride);
  EXPECT_TRUE(consumer_.contentLoading);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(preferred_identity_.userEmail, consumer_.selectedEmail);
  EXPECT_NSEQ(preferred_identity_.userEmail,
              consumer_.selectedEmailWhenContentLoadingStopped);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(),
              consumer_.subtitleOverrideWhenContentLoadingStopped);
  EXPECT_FALSE(consumer_.contentLoading);
}

// Tests that the default identity is kept and the loading state is hidden if
// the preferred account request times out (resolves with `std::nullopt`).
TEST_F(FullscreenSigninScreenMediatorTest,
       KeepsDefaultIdentityOnPreferredAccountTimeout) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_TRUE(consumer_.contentLoading);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      std::nullopt);
  EXPECT_FALSE(consumer_.contentLoading);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that the timeout configured in
// `kAccountPreviewPreferredAccountFREFetchTimeout` is passed to
// `GetPreferredAccountForSigninAsync()`.
TEST_F(FullscreenSigninScreenMediatorTest,
       PassesConfiguredPreferredAccountFetchTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewPreferredAccountFRE,
      {{switches::kAccountPreviewPreferredAccountFREFetchTimeout.name, "3s"}});

  CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  EXPECT_EQ(
      base::Seconds(3),
      account_preview_data_service_.last_preferred_account_request_timeout());
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
  EXPECT_TRUE(consumer_.contentLoading);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
  EXPECT_FALSE(consumer_.contentLoading);
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
  EXPECT_FALSE(consumer_.contentLoading);
}

// Tests that the preferred account is ignored and the loading state is hidden
// once the mediator is disconnected.
TEST_F(FullscreenSigninScreenMediatorTest,
       IgnoresPreferredAccountAfterDisconnect) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_TRUE(consumer_.contentLoading);

  [mediator disconnect];
  EXPECT_FALSE(consumer_.contentLoading);

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
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));
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

// Tests that `subtitleOverride` is not updated when the preferred account is
// selected as a result of the initially selected identity being removed from
// the device.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotUpdateSubtitleWhenPreferredIdentitySelectedOnAccountRemoval) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);

  // The preferred account becomes available after initial load, and the
  // currently selected `default_identity_` is removed from the device.
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that waiting for the preferred account stops, the loading state is
// hidden, and the pending callback is cancelled if all accounts on the device
// are removed while waiting.
TEST_F(FullscreenSigninScreenMediatorTest,
       StopsWaitingWhenAllAccountsRemovedInFRE) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.set_defer_preferred_account_requests(true);
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_TRUE(consumer_.contentLoading);

  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      preferred_identity_);

  EXPECT_EQ(nil, mediator.selectedIdentity);
  EXPECT_FALSE(consumer_.contentLoading);

  // Re-adding identities and completing the pending request should not select
  // `preferred_identity_` because the request was cancelled.
  GetSystemIdentityManager()->AddIdentity(default_identity_);
  GetSystemIdentityManager()->AddIdentity(preferred_identity_);
  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(preferred_identity_.gaiaId));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
}

// Tests that `subtitleOverride` is not set when sync is disabled by enterprise
// policy.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotSetSubtitleOverrideWhenSyncDisabledByPolicy) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));
  static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()))
      ->SetAllowedByEnterprisePolicy(false);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_FALSE(consumer_.syncEnabled);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that `subtitleOverride` is not set when sign-in is forced by enterprise
// policy.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotSetSubtitleOverrideWhenSigninForcedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));
  GetApplicationContext()->GetLocalState()->SetInteger(
      prefs::kBrowserSigninPolicy,
      static_cast<int>(BrowserSigninMode::kForced));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_EQ(SigninScreenConsumerSigninStatusForced, consumer_.signinStatus);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that `subtitleOverride` is updated when the default (or only) identity
// on the device is also the preferred account and finishes computing
// asynchronously in the FRE.
TEST_F(FullscreenSigninScreenMediatorTest,
       UpdatesSubtitleOverrideWhenDefaultIdentityIsPreferredInFRE) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      preferred_identity_);
  account_preview_data_service_.set_defer_preferred_account_requests(true);

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
  EXPECT_TRUE(consumer_.contentLoading);

  account_preview_data_service_.RunPendingPreferredAccountRequests(
      CreatePreference(default_identity_.gaiaId));

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(),
              consumer_.subtitleOverrideWhenContentLoadingStopped);
  EXPECT_FALSE(consumer_.contentLoading);
}

// Tests that manually switching the selected identity does not change
// `subtitleOverride`.
TEST_F(FullscreenSigninScreenMediatorTest,
       KeepsSubtitleOverrideWhenUserManuallySwitchesIdentity) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !consumer_.contentLoading; }));
  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);

  mediator.selectedIdentity = default_identity_;

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_NSEQ(ExpectedDefaultSubtitleOverride(), consumer_.subtitleOverride);
}

// Tests that flows other than FRE and the fullscreen sign-in promo still
// select the preferred account when `kEnableAccountPreviewPreferredAccount` is
// enabled, without setting `subtitleOverride`.
TEST_F(FullscreenSigninScreenMediatorTest,
       SelectsPreferredAccountWithoutSubtitleOverrideInOtherFlows) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kEnableAccountPreviewPreferredAccount,
                            switches::kEnableAccountPreviewPreferredAccountFRE},
      /*disabled_features=*/{});
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  FullscreenSigninScreenMediator* mediator = CreateMediatorWithConsumer(
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup);

  EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that the deeplink flow keeps the externally passed `selectedIdentity`
// and does not select the preferred account or set `subtitleOverride`.
TEST_F(FullscreenSigninScreenMediatorTest,
       DoesNotSelectPreferredAccountOrSetSubtitleOverrideInDeeplinkFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kEnableAccountPreviewPreferredAccount,
                            switches::kEnableAccountPreviewPreferredAccountFRE},
      /*disabled_features=*/{});
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  FullscreenSigninScreenMediator* mediator = CreateMediatorWithConsumer(
      signin_metrics::AccessPoint::kDeepLinkDefault, default_identity_);

  EXPECT_NSEQ(default_identity_, mediator.selectedIdentity);
  EXPECT_EQ(nil, consumer_.subtitleOverride);
}

// Tests that `SignedInWithDefaultAccount` is recorded when signing in with the
// default account in the FRE.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWhenSignedInWithDefaultAccount) {
  base::HistogramTester histogram_tester;
  CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  SimulateSignInWithIdentity(default_identity_,
                             signin_ui::CancelationReason::kNotCanceled);

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_DEFAULT_ACCOUNT,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount",
      signin_metrics::AccessPoint::kStartPage, 1);
}

// Tests that `SignedInWithDefaultAccount` is recorded when a preferred account
// is pre-selected in the FRE and the user signs in with it, and that
// `SignedInWithNonDefaultAccount` is recorded if the user switches back to the
// first device account.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWithPreferredAccountInFRE) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAccountPreviewPreferredAccountFRE);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePreference(preferred_identity_.gaiaId));

  {
    base::HistogramTester histogram_tester;
    FullscreenSigninScreenMediator* mediator =
        CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !consumer_.contentLoading; }));
    EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);

    SimulateSignInWithIdentity(preferred_identity_,
                               signin_ui::CancelationReason::kNotCanceled);

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction",
        signin_metrics::AccountConsistencyPromoAction::
            SIGNED_IN_WITH_DEFAULT_ACCOUNT,
        1);
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount",
        signin_metrics::AccessPoint::kStartPage, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount",
        0);
    [mediator disconnect];
    mediator_ = nil;
  }

  {
    consumer_ = [[FakeFullscreenSigninScreenConsumer alloc] init];
    base::HistogramTester histogram_tester;
    FullscreenSigninScreenMediator* mediator =
        CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !consumer_.contentLoading; }));
    EXPECT_NSEQ(preferred_identity_, mediator.selectedIdentity);

    mediator.selectedIdentity = default_identity_;
    SimulateSignInWithIdentity(default_identity_,
                               signin_ui::CancelationReason::kNotCanceled);

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction",
        signin_metrics::AccountConsistencyPromoAction::
            SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
        1);
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount",
        signin_metrics::AccessPoint::kStartPage, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount", 0);
  }
}

// Tests that `SignedInWithNonDefaultAccount` is recorded when signing in with a
// non-default account in the FRE.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWhenSignedInWithNonDefaultAccount) {
  base::HistogramTester histogram_tester;
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  mediator.selectedIdentity = preferred_identity_;

  SimulateSignInWithIdentity(preferred_identity_,
                             signin_ui::CancelationReason::kNotCanceled);

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount",
      signin_metrics::AccessPoint::kStartPage, 1);
}

// Tests that `SignedInWithAddedAccount` is recorded when signing in with an
// account added during the FRE flow, and that `SignedInWithDefaultAccount` is
// recorded if the user adds an account and then switches back to the default
// account.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWhenSignedInWithAddedAccount) {
  FakeSystemIdentity* added_identity = [FakeSystemIdentity fakeIdentity3];
  GetSystemIdentityManager()->AddIdentity(added_identity);

  {
    base::HistogramTester histogram_tester;
    FullscreenSigninScreenMediator* mediator =
        CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
    [mediator systemIdentityAdded:added_identity];
    mediator.selectedIdentity = added_identity;

    SimulateSignInWithIdentity(added_identity,
                               signin_ui::CancelationReason::kNotCanceled);

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction",
        signin_metrics::AccountConsistencyPromoAction::
            SIGNED_IN_WITH_ADDED_ACCOUNT,
        1);
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount",
        signin_metrics::AccessPoint::kStartPage, 1);
    [mediator disconnect];
    mediator_ = nil;
  }

  {
    consumer_ = [[FakeFullscreenSigninScreenConsumer alloc] init];
    base::HistogramTester histogram_tester;
    FullscreenSigninScreenMediator* mediator =
        CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
    [mediator systemIdentityAdded:added_identity];
    mediator.selectedIdentity = added_identity;
    mediator.selectedIdentity = default_identity_;

    SimulateSignInWithIdentity(default_identity_,
                               signin_ui::CancelationReason::kNotCanceled);

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction",
        signin_metrics::AccountConsistencyPromoAction::
            SIGNED_IN_WITH_DEFAULT_ACCOUNT,
        1);
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount",
        signin_metrics::AccessPoint::kStartPage, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount", 0);
  }
}

// Tests that `SignedInWithNoDeviceAccount` is recorded when the screen starts
// with no accounts on the device and the user adds an account and signs in.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWhenSignedInWithNoDeviceAccount) {
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      default_identity_);
  GetSystemIdentityManager()->ForgetIdentityFromOtherApplication(
      preferred_identity_);

  base::HistogramTester histogram_tester;
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);
  EXPECT_EQ(nil, mediator.selectedIdentity);

  FakeSystemIdentity* added_identity = [FakeSystemIdentity fakeIdentity3];
  GetSystemIdentityManager()->AddIdentity(added_identity);
  [mediator systemIdentityAdded:added_identity];
  mediator.selectedIdentity = added_identity;

  SimulateSignInWithIdentity(added_identity,
                             signin_ui::CancelationReason::kNotCanceled);

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_NO_DEVICE_ACCOUNT,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction.SignedInWithNoDeviceAccount",
      signin_metrics::AccessPoint::kStartPage, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount", 0);
}

// Tests that `DismissedButton` is recorded when the sign-in screen is canceled
// in the FRE.
TEST_F(FullscreenSigninScreenMediatorTest,
       RecordsHistogramsWhenDismissedWithButton) {
  base::HistogramTester histogram_tester;
  FullscreenSigninScreenMediator* mediator =
      CreateMediatorWithConsumer(signin_metrics::AccessPoint::kStartPage);

  [mediator cancelSignInScreenWithCompletion:nil];

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAction.DismissedButton",
      signin_metrics::AccessPoint::kStartPage, 1);
}
