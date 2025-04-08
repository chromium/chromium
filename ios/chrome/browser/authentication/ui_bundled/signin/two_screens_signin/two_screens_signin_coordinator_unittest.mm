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
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

// Test cases for the TwoScreensSigninCoordinator.
class TwoScreensSigninCoordinatorTest : public PlatformTest {
 public:
  TwoScreensSigninCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

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
                                       PROMO_ACTION_NO_SIGNIN_PROMO];
    coordinator_.signinCompletion = ^(
        SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
      EXPECT_EQ(signinResult, expected_result);
      EXPECT_EQ(expected_signin_completion_identity, signinCompletionIdentity);
      completion_block_done_ = true;
    };
    EXPECT_EQ(PresentedViewController(), nil);
    [coordinator_ start];
  }

  ~TwoScreensSigninCoordinatorTest() override { [coordinator_ stop]; }

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
  void SigninFakeIdentity() {
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
  }

  // Advances the coordinator to the next screen.
  void NextScreen() {
    [coordinator_ screenWillFinishPresenting];
    // Spin the run loop to allow screen to change.
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(100));
  }

 protected:
  bool completion_block_done_ = false;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  TwoScreensSigninCoordinator* coordinator_;
  base::UserActionTester user_actions_;
  UIWindow* window_;
  FakeSystemIdentity* fake_identity_ = nil;
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
  SigninFakeIdentity();

  NextScreen();

  // Expect the history sync opt-in screen to be presented.
  EXPECT_TRUE(
      [TopViewController() isKindOfClass:[HistorySyncViewController class]]);

  // Shut it down.
  [coordinator_ interruptAnimated:YES];
  [coordinator_ stop];
  // Expect completion block to be run synchronously and be finished when
  // -stop returns.
  EXPECT_TRUE(completion_block_done_);
  ExpectNoUpgradePromoHistogram(&histogram_tester);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kSettings, 1);
  histogram_tester.ExpectUniqueSample<signin_metrics::AccessPoint>(
      "Signin.SigninStartedAccessPoint", signin_metrics::AccessPoint::kSettings,
      1);
}

// Tests that stopping the coordinator before it is done will interrupt it.
TEST_F(TwoScreensSigninCoordinatorTest, StopWillInterrupt) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultInterrupted, nil);
  [coordinator_ stop];

  // Expect completion block to be run synchronously and be finished when
  // -stop returns.
  EXPECT_TRUE(completion_block_done_);

  ExpectNoUpgradePromoHistogram(&histogram_tester);
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
  [coordinator_ stop];
  ExpectNoUpgradePromoHistogram(&histogram_tester);
}

// Tests that the user can swipe to dismiss and that a user action is recorded.
TEST_F(TwoScreensSigninCoordinatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  StartTwoScreensSigninCoordinator(SigninCoordinatorResultInterrupted, nil);

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

  [coordinator_ stop];
  ExpectNoUpgradePromoHistogram(&histogram_tester);
}
