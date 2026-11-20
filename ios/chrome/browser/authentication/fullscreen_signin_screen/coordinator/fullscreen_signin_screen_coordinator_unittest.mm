// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/browser/account_switching_promo_metrics.h"
#import "components/signin/public/browser/account_switching_promo_prefs.h"
#import "components/sync/test/test_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Testing factory returning a `NiceMock<MockTracker>` for `profile`.
std::unique_ptr<KeyedService> BuildMockFeatureEngagementTracker(
    ProfileIOS& profile) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}

// Test fixture for `FullscreenSigninScreenCoordinator`.
class FullscreenSigninScreenCoordinatorTest : public PlatformTest {
 public:
  FullscreenSigninScreenCoordinatorTest() {
    TestProfileIOS::Builder builder;
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] init];
    scene_state_.profileState = profile_state_;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(&BuildMockFeatureEngagementTracker));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    snackbar_commands_mock_ =
        OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_commands_mock_
                     forProtocol:@protocol(SnackbarCommands)];

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  }

  ~FullscreenSigninScreenCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY(snackbar_commands_mock_);
  }

 protected:
  // Returns the mock feature engagement tracker attached to `profile_`.
  feature_engagement::test::MockTracker* GetMockTracker() {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
  }

  // Returns the fake `SystemIdentityManager` from the application context.
  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  id snackbar_commands_mock_ = nil;
};

// Tests that UI actions triggered after stopping the coordinator are safely
// ignored and do not crash.
TEST_F(FullscreenSigninScreenCoordinatorTest, ActionsAfterStopDoNotCrash) {
  UINavigationController* navigation_controller =
      [[UINavigationController alloc] init];
  FullscreenSigninScreenCoordinator* coordinator =
      [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:navigation_controller
                                    browser:browser_.get()
                                   delegate:nil
                               contextStyle:SigninContextStyle::kDefault
                                accessPoint:signin_metrics::AccessPoint::
                                                kFullscreenSigninPromo
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];

  [coordinator start];

  FullscreenSigninScreenViewController* view_controller =
      base::apple::ObjCCast<FullscreenSigninScreenViewController>(
          navigation_controller.topViewController);
  ASSERT_NE(nil, view_controller);

  [coordinator stop];

  // UI callbacks triggered after `-stop` must be safely ignored.
  id<ButtonStackActionDelegate> action_delegate =
      static_cast<id<ButtonStackActionDelegate>>(view_controller);
  [action_delegate didTapPrimaryActionButton];
  [action_delegate didTapSecondaryActionButton];
}

// Tests that `FullscreenSigninScreenViewController` updates its subtitle when
// `subtitleOverride` is set.
TEST_F(FullscreenSigninScreenCoordinatorTest,
       UpdatesSubtitleWhenSubtitleOverrideChanges) {
  UINavigationController* navigation_controller =
      [[UINavigationController alloc] init];
  FullscreenSigninScreenCoordinator* coordinator =
      [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:navigation_controller
                                    browser:browser_.get()
                                   delegate:nil
                               contextStyle:SigninContextStyle::kDefault
                                accessPoint:signin_metrics::AccessPoint::
                                                kFullscreenSigninPromo
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];

  [coordinator start];

  FullscreenSigninScreenViewController* view_controller =
      base::apple::ObjCCast<FullscreenSigninScreenViewController>(
          navigation_controller.topViewController);
  ASSERT_NE(nil, view_controller);
  [view_controller loadViewIfNeeded];

  NSString* default_subtitle =
      l10n_util::GetNSString(IDS_IOS_UNO_UPGRADE_PROMO_SIGNIN_SUBTITLE);
  EXPECT_NSEQ(default_subtitle, view_controller.subtitleText);
  EXPECT_NSEQ(default_subtitle, view_controller.subtitleLabel.text);

  NSString* subtitle_override = @"Custom subtitle for preferred account";
  view_controller.subtitleOverride = subtitle_override;
  EXPECT_NSEQ(subtitle_override, view_controller.subtitleText);
  EXPECT_NSEQ(subtitle_override, view_controller.subtitleLabel.text);

  [coordinator stop];
}

// Tests that the account switching promo screen loads, emits
// `kPromoTriggered` on start, and records `kIgnored` when stopped
// without user action.
TEST_F(FullscreenSigninScreenCoordinatorTest,
       AccountSwitchingPromoRecordsIgnoredOnStop) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableAccountPreviewSwitchingAccount);
  // Sign in with `fakeIdentity1` and add `fakeIdentity2` on the device as the
  // promo's target.
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn([FakeSystemIdentity fakeIdentity1],
               signin_metrics::AccessPoint::kStartPage);
  FakeSystemIdentity* target_identity = [FakeSystemIdentity fakeIdentity2];
  GetSystemIdentityManager()->AddIdentity(target_identity);

  UINavigationController* navigation_controller =
      [[UINavigationController alloc] init];
  FullscreenSigninScreenCoordinator* coordinator =
      [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:navigation_controller
                                    browser:browser_.get()
                                   delegate:nil
                               contextStyle:SigninContextStyle::
                                                kAccountSwitchingPromo
                                accessPoint:signin_metrics::AccessPoint::
                                                kAccountSwitchingPromo
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];
  coordinator.identity = target_identity;
  coordinator.canSwitchAccount = YES;

  base::HistogramTester histogram_tester;
  // Simulate the promos manager presenting the account switching promo.
  [coordinator start];
  histogram_tester.ExpectUniqueSample(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kPromoTriggered, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::kAccountSwitchingPromo, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SigninStartedAccessPoint",
      signin_metrics::AccessPoint::kAccountSwitchingPromo, 1);
  histogram_tester.ExpectTotalCount("IOS.SignInpromo.Fullscreen.PromoEvents",
                                    0);

  // The account switching promo has no disclaimer.
  FullscreenSigninScreenViewController* view_controller =
      base::apple::ObjCCast<FullscreenSigninScreenViewController>(
          navigation_controller.topViewController);
  ASSERT_NE(nil, view_controller);
  [view_controller loadViewIfNeeded];
  EXPECT_EQ(0u, view_controller.disclaimerText.length);

  EXPECT_CALL(
      *GetMockTracker(),
      NotifyEvent(feature_engagement::events::kAccountSwitchingPromoIgnored))
      .Times(1);
  // Simulate the promo being stopped without any user action.
  [coordinator stop];
  histogram_tester.ExpectBucketCount(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kIgnored, 1);
  histogram_tester.ExpectTotalCount(
      signin::kAccountSwitchingPromoEventsHistogram, 2);
}

// Tests that tapping the secondary ("Not now") action button explicitly
// dismisses the account switching promo, records `kExplicitlyDismissed` and the
// dismissal pref, and asks the screen delegate to stop the coordinator.
TEST_F(FullscreenSigninScreenCoordinatorTest,
       AccountSwitchingPromoExplicitDismissRecordsOutcome) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableAccountPreviewSwitchingAccount);
  FakeSystemIdentity* primary_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target_identity = [FakeSystemIdentity fakeIdentity2];
  // Sign in with `primary_identity` and add `target_identity` on the device.
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(primary_identity, signin_metrics::AccessPoint::kStartPage);
  GetSystemIdentityManager()->AddIdentity(target_identity);

  id screen_delegate = OCMStrictProtocolMock(@protocol(FirstRunScreenDelegate));
  UINavigationController* navigation_controller =
      [[UINavigationController alloc] init];
  FullscreenSigninScreenCoordinator* coordinator =
      [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:navigation_controller
                                    browser:browser_.get()
                                   delegate:screen_delegate
                               contextStyle:SigninContextStyle::
                                                kAccountSwitchingPromo
                                accessPoint:signin_metrics::AccessPoint::
                                                kAccountSwitchingPromo
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];
  coordinator.identity = target_identity;
  coordinator.canSwitchAccount = YES;

  base::HistogramTester histogram_tester;
  // Simulate the promos manager presenting the account switching promo.
  [coordinator start];

  FullscreenSigninScreenViewController* view_controller =
      base::apple::ObjCCast<FullscreenSigninScreenViewController>(
          navigation_controller.topViewController);
  ASSERT_NE(nil, view_controller);
  [view_controller loadViewIfNeeded];

  OCMExpect(
      [screen_delegate firstRunScreenCoordinatorWantsToBeStopped:coordinator]);
  EXPECT_CALL(*GetMockTracker(),
              NotifyEvent(feature_engagement::events::
                              kAccountSwitchingPromoExplicitlyDismissed))
      .Times(1);

  // Simulate the user tapping "Not now" and the coordinator being stopped.
  id<ButtonStackActionDelegate> action_delegate =
      static_cast<id<ButtonStackActionDelegate>>(view_controller);
  [action_delegate didTapSecondaryActionButton];
  [coordinator stop];

  EXPECT_OCMOCK_VERIFY(screen_delegate);
  histogram_tester.ExpectBucketCount(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kExplicitlyDismissed, 1);
  histogram_tester.ExpectBucketCount(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kIgnored, 0);

  // One more explicit dismissal reaches the cap of 2, proving the first
  // dismissal was recorded in profile prefs.
  EXPECT_TRUE(signin::CanShowAccountSwitchingPromo(
      *profile_->GetPrefs(), primary_identity.gaiaId, target_identity.gaiaId));
  signin::RecordAccountSwitchingPromoExplicitlyDismissed(
      *profile_->GetPrefs(), primary_identity.gaiaId, target_identity.gaiaId);
  EXPECT_FALSE(signin::CanShowAccountSwitchingPromo(
      *profile_->GetPrefs(), primary_identity.gaiaId, target_identity.gaiaId));
}

// Tests that accepting the account switching promo switches the primary
// account, records `kAccepted`, reverse-pair suppression, and 24-hour retention
// state in profile prefs, and stops the coordinator without showing a
// snackbar.
TEST_F(FullscreenSigninScreenCoordinatorTest,
       AccountSwitchingPromoAcceptedRecordsOutcome) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableAccountPreviewSwitchingAccount);
  FakeSystemIdentity* primary_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target_identity = [FakeSystemIdentity fakeIdentity2];
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  // Sign in with `primary_identity` and add `target_identity` on the device.
  auth_service->SignIn(primary_identity,
                       signin_metrics::AccessPoint::kStartPage);
  GetSystemIdentityManager()->AddIdentity(target_identity);

  id screen_delegate = OCMStrictProtocolMock(@protocol(FirstRunScreenDelegate));
  UINavigationController* navigation_controller =
      [[UINavigationController alloc] init];
  FullscreenSigninScreenCoordinator* coordinator =
      [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:navigation_controller
                                    browser:browser_.get()
                                   delegate:screen_delegate
                               contextStyle:SigninContextStyle::
                                                kAccountSwitchingPromo
                                accessPoint:signin_metrics::AccessPoint::
                                                kAccountSwitchingPromo
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];
  coordinator.identity = target_identity;
  coordinator.canSwitchAccount = YES;

  base::HistogramTester histogram_tester;
  // Simulate the promos manager presenting the account switching promo.
  [coordinator start];

  // Simulate the promo screen appearing without requiring a scroll to the end
  // before the buttons are enabled.
  FullscreenSigninScreenViewController* view_controller =
      base::apple::ObjCCast<FullscreenSigninScreenViewController>(
          navigation_controller.topViewController);
  ASSERT_NE(nil, view_controller);
  [view_controller loadViewIfNeeded];
  view_controller.scrollToEndMandatory = NO;
  [view_controller viewWillAppear:NO];

  __block BOOL wants_to_be_stopped = NO;
  OCMExpect(
      [screen_delegate firstRunScreenCoordinatorWantsToBeStopped:coordinator])
      .andDo(^(NSInvocation*) {
        wants_to_be_stopped = YES;
      });

  // Before any user action, the user can swipe the promo down.
  id<UIAdaptivePresentationControllerDelegate> presentation_delegate =
      navigation_controller.presentationController.delegate;
  EXPECT_TRUE([presentation_delegate
      presentationControllerShouldDismiss:navigation_controller
                                              .presentationController]);

  // Simulate the user tapping the primary button to switch account.
  id<ButtonStackActionDelegate> action_delegate =
      static_cast<id<ButtonStackActionDelegate>>(view_controller);
  [action_delegate didTapPrimaryActionButton];
  // While the account switch is in progress, swipe-to-dismiss is disabled.
  EXPECT_FALSE([presentation_delegate
      presentationControllerShouldDismiss:navigation_controller
                                              .presentationController]);

  // Wait for the account switch to complete and the coordinator to ask its
  // delegate to be stopped.
  ASSERT_TRUE(base::test::RunUntil(^bool {
    return wants_to_be_stopped;
  }));
  [coordinator stop];

  EXPECT_OCMOCK_VERIFY(screen_delegate);
  EXPECT_NSEQ(target_identity, auth_service->GetPrimaryIdentity());
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::kAccountSwitchingPromo, 1);
  histogram_tester.ExpectBucketCount(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kIgnored, 0);
  EXPECT_FALSE(signin::CanShowAccountSwitchingPromo(
      *profile_->GetPrefs(), target_identity.gaiaId, primary_identity.gaiaId));
  // Simulate the user switching back to `primary_identity` within 24 hours.
  signin::MaybeRecordAccountSwitchingRetention(*profile_->GetPrefs(),
                                               primary_identity.gaiaId);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", true, 1);
}

}  // namespace
