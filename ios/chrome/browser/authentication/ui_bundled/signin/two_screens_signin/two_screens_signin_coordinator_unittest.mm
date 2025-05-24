// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/two_screens_signin/two_screens_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test cases for the TwoScreensSigninCoordinator.
class TwoScreensSigninCoordinatorTest : public PlatformTest {
 public:
  TwoScreensSigninCoordinatorTest() {
    TestProfileIOS::Builder builder;
    // The profile state will receive UI blocker request. They are not tested
    // here, so it’s a non-strict mock.
    profile_state_ = OCMClassMock([ProfileState class]);
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state_;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
    [standardDefaults removeObjectForKey:kDisplayedSSORecallPromoCountKey];
    [standardDefaults removeObjectForKey:kDisplayedSSORecallForMajorVersionKey];
    [standardDefaults removeObjectForKey:kLastShownAccountGaiaIdVersionKey];
    [standardDefaults removeObjectForKey:kSigninPromoViewDisplayCountKey];

    UIView.animationsEnabled = NO;
    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    // Resets all preferences related to upgrade promo.
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity_);
  }

  ~TwoScreensSigninCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)profile_state_);
  }

  // Initalize coordinator_ up to start.
  // Expects it receives a completion with the expected_result and
  // expected_signin_completion_identity_
  void StartTwoScreensSigninCoordinator(
      SigninCoordinatorResult expected_result,
      id<SystemIdentity> expected_signin_completion_identity) {
    coordinator_ = [[TwoScreensSigninCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                      contextStyle:SigninContextStyle::kDefault
                       accessPoint:signin_metrics::AccessPoint::kSettings
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NO_SIGNIN_PROMO
              continuationProvider:NotReachedContinuationProvider()];
    coordinator_.signinCompletion = ^(
        SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
      EXPECT_EQ(signinResult, expected_result);
      EXPECT_EQ(expected_signin_completion_identity, signinCompletionIdentity);
      StopCoordinator();
      completion_block_done_ = true;
    };
    EXPECT_EQ(PresentedViewController(), nil);
    [coordinator_ start];
  }

  // Returns the presentedViewController.
  UIViewController* PresentedViewController() {
    return window_.rootViewController.presentedViewController;
  }

  // Returns the presented navigation controller's topViewController.
  UIViewController* TopViewController() {
    UIViewController* presented = PresentedViewController();
    return base::apple::ObjCCast<UINavigationController>(presented)
        .topViewController;
  }

  // Expects no preferences or metrics related to upgrade promo since the access
  // point is not `kSigninPromo`.
  void ExpectNoUpgradePromoHistogram(base::HistogramTester* histogram_tester) {
    histogram_tester->ExpectTotalCount(kUMASSORecallAccountsAvailable, 0);
    histogram_tester->ExpectTotalCount(kUMASSORecallPromoSeenCount, 0);
    histogram_tester->ExpectTotalCount(kUMASSORecallPromoAction, 0);
    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
    EXPECT_EQ([standardDefaults integerForKey:kDisplayedSSORecallPromoCountKey],
              0);
    EXPECT_EQ(
        [standardDefaults objectForKey:kDisplayedSSORecallForMajorVersionKey],
        nil);
    EXPECT_EQ([standardDefaults objectForKey:kLastShownAccountGaiaIdVersionKey],
              nil);
    EXPECT_EQ([standardDefaults integerForKey:kSigninPromoViewDisplayCountKey],
              0);
  }

  // Signs in a fake identity.
  void SigninFakeIdentity(bool has_history_sync_opt_in) {
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile_.get());
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, has_history_sync_opt_in);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, has_history_sync_opt_in);
  }

  // Advances the coordinator to the next screen.
  void NextScreen() {
    [coordinator_ screenWillFinishPresenting];
    // Spin the run loop to allow screen to change.
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(100));
  }

 protected:
  // Stops the coordinator and unset it.
  void StopCoordinator() {
    EXPECT_TRUE(scene_state_.signinInProgress);
    [coordinator_ stop];
    coordinator_ = nil;
  }

  bool completion_block_done_ = false;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  TwoScreensSigninCoordinator* coordinator_;
  base::UserActionTester user_actions_;
  UIWindow* window_;
  FakeSystemIdentity* fake_identity_ = nil;
  SceneState* scene_state_;

 private:
  // Required for UI blocker.
  ProfileState* profile_state_;
};

#pragma mark - Tests

// Tests that the screens are presented.
TEST_F(TwoScreensSigninCoordinatorTest, PresentScreens) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultInterrupted, nil);
  // Expect the signin screen to be presented.
  EXPECT_NE(PresentedViewController(), nil);
  EXPECT_TRUE([TopViewController()
      isKindOfClass:[FullscreenSigninScreenViewController class]]);
  SigninFakeIdentity(/*has_history_sync_opt_in=*/false);

  NextScreen();

  // Expect the history sync opt-in screen to be presented.
  EXPECT_TRUE(
      [TopViewController() isKindOfClass:[HistorySyncViewController class]]);

  // Shut it down.
  StopCoordinator();
  // Expect completion block not to be run when the stop comes from an external
  // caller.
  EXPECT_FALSE(completion_block_done_);
  ExpectNoUpgradePromoHistogram(&histogram_tester);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kSettings, 1);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SigninStartedAccessPoint", signin_metrics::AccessPoint::kSettings,
      1);
  EXPECT_FALSE(scene_state_.signinInProgress);
}

// Tests that the screens are not presented when the user has already signed in
// and history sync opt-in.
TEST_F(TwoScreensSigninCoordinatorTest,
       ScreensNotPresentedWhenSignedInHistorySyncOptIn) {
  base::HistogramTester histogram_tester;
  SigninFakeIdentity(/*has_history_sync_opt_in=*/true);

  StartTwoScreensSigninCoordinator(SigninCoordinatorResultSuccess,
                                   fake_identity_);
  // Expect the signin screen to not be presented.
  EXPECT_EQ(PresentedViewController(), nil);
  EXPECT_FALSE([TopViewController()
      isKindOfClass:[FullscreenSigninScreenViewController class]]);
  // Expect the history sync screen to not be presented.
  EXPECT_FALSE(
      [TopViewController() isKindOfClass:[HistorySyncViewController class]]);

  // Expect completion block to be run synchronously and be finished without
  // calling -stop. Since the user has already signed in and history sync
  // opt-in, the coordinator will call the completion block.
  EXPECT_TRUE(completion_block_done_);
  ExpectNoUpgradePromoHistogram(&histogram_tester);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kSettings, 0);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SigninStartedAccessPoint", signin_metrics::AccessPoint::kSettings,
      0);
  EXPECT_FALSE(scene_state_.signinInProgress);
}

// Tests that stopping the coordinator before it is done will interrupt it.
TEST_F(TwoScreensSigninCoordinatorTest, StopWillInterrupt) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultInterrupted, nil);

  StopCoordinator();
  // Expect completion block not to be run when the stop comes from an external
  // caller.
  EXPECT_FALSE(completion_block_done_);

  ExpectNoUpgradePromoHistogram(&histogram_tester);
  EXPECT_FALSE(scene_state_.signinInProgress);
}

// Tests that the user can cancel without signing in.
TEST_F(TwoScreensSigninCoordinatorTest, CanceledByUser) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultCanceledByUser, nil);

  [coordinator_ screenWillFinishPresenting];

  auto completion_condition = ^{
    return completion_block_done_;
  };
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), true, completion_condition));
  ExpectNoUpgradePromoHistogram(&histogram_tester);
  EXPECT_FALSE(scene_state_.signinInProgress);
}

// Tests that the user can swipe to dismiss and that a user action is recorded.
TEST_F(TwoScreensSigninCoordinatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultCanceledByUser, nil);

  // Simulate a swipe-to-dismiss.
  EXPECT_EQ(0, user_actions_.GetActionCount("Signin_TwoScreens_SwipeDismiss"));
  UIPresentationController* presentationController =
      PresentedViewController().presentationController;
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  auto completion_condition = ^{
    return completion_block_done_;
  };
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), true, completion_condition));
  EXPECT_EQ(1, user_actions_.GetActionCount("Signin_TwoScreens_SwipeDismiss"));

  ExpectNoUpgradePromoHistogram(&histogram_tester);
  EXPECT_FALSE(scene_state_.signinInProgress);
}
