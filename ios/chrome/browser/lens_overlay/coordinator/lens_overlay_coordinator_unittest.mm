// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

@interface LensOverlayCoordinator ()
- (BOOL)isUICreated;
@end

namespace {

class LensOverlayCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
      GTEST_SKIP() << "Feature unsupported on iPad";
    }

    feature_list_.InitAndEnableFeature(kEnableLensOverlay);

    root_view_controller_ = [[UIViewController alloc] init];
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;

    // Browser state
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_, std::make_unique<FakeAuthenticationServiceDelegate>());

    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_);

    browser_ = std::make_unique<TestBrowser>(profile_);
    dispatcher_ = [[CommandDispatcher alloc] init];

    GetApplicationContext()->GetLocalState()->SetInteger(
        lens::prefs::kLensOverlaySettings,
        static_cast<int>(
            lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

    base_view_controller_ = [[UIViewController alloc] init];

    // LensOverlayCoordinator
    coordinator_ = [[LensOverlayCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    [dispatcher_ startDispatchingToTarget:coordinator_
                              forProtocol:@protocol(LensOverlayCommands)];

    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    // Tab helper
    web_state_ = std::make_unique<web::FakeWebState>();
    LensOverlayTabHelper::CreateForWebState(web_state_.get());
    SnapshotTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = LensOverlayTabHelper::FromWebState(web_state_.get());

    // Attach SnapshotTabHelper to allow snapshot generation.
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    SnapshotTabHelper::FromWebState(web_state_.get())->SetDelegate(delegate_);

    // Add a fake view to the delgate, which will be used to capture snapshots.
    CGRect frame = {CGPointZero, CGSizeMake(300, 400)};
    delegate_.view = [[UIView alloc] initWithFrame:frame];
    delegate_.view.backgroundColor = [UIColor blueColor];

    // Mark the only web state as active.
    browser_.get()->GetWebStateList()->InsertWebState(std::move(web_state_));
    browser_.get()->GetWebStateList()->ActivateWebStateAt(0);

    // Increment the fullscreen disabled counter.
    FullscreenController* fullscreen_controller =
        FullscreenController::FromBrowser(browser_.get());
    fullscreen_controller->IncrementDisabledCounter();

    // Log in with a fake identity.
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_system_identity_manager->AddIdentity(identity);
    authentication_service->SignIn(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

    // Wait for the base view controller to be presented.
    base_view_controller_.modalPresentationStyle =
        UIModalPresentationOverCurrentContext;
    __block bool presentation_finished = NO;
    [root_view_controller_ presentViewController:base_view_controller_
                                        animated:NO
                                      completion:^{
                                        presentation_finished = YES;
                                      }];
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return presentation_finished;
    }));
  }

  void TearDown() override {
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
      // Dismisses `base_view_controller_` and waits for the dismissal to
      // finish.
      __block bool dismissal_finished = NO;
      [root_view_controller_ dismissViewControllerAnimated:NO
                                                completion:^{
                                                  dismissal_finished = YES;
                                                }];
      EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
        return dismissal_finished;
      }));
    }

    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  FakeSnapshotGeneratorDelegate* delegate_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  LensOverlayCoordinator* coordinator_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::WebState> web_state_;
  UIViewController* base_view_controller_;
  base::test::ScopedFeatureList feature_list_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  id dispatcher_;
  raw_ptr<LensOverlayTabHelper> tab_helper_;
  id<ApplicationCommands> application_handler_;

  void DeliverMemoryWarningNotification() {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIApplicationDidReceiveMemoryWarningNotification
                      object:nil];
  }
};

// The overlay should appear shown only after the UI is created.
TEST_F(LensOverlayCoordinatorTest, ShouldMarkOverlayShownWhenUICreated) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // Then the UI should not be shown to the user.
  EXPECT_FALSE(tab_helper_->IsLensOverlayShown());

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kLocationBar
               completion:nil];

  // Then the UI should appear created and shown to the user.
  EXPECT_TRUE(tab_helper_->IsLensOverlayShown());
}

// When the UI is destroyed the overlay should not appear shown.
TEST_F(LensOverlayCoordinatorTest, ShouldDestroyTheUIUponRequest) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kLocationBar
               completion:nil];

  // Then the UI should appear created and shown to the user.
  EXPECT_TRUE(tab_helper_->IsLensOverlayShown());

  // When the destroy command is dispatched.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      destroyLensUI:NO
             reason:lens::LensOverlayDismissalSource::kOverlayCloseButton];

  // Then the UI should not appear shown anymore.
  EXPECT_FALSE(tab_helper_->IsLensOverlayShown());
}

// When the UI is not created the `show` command should do nothing.
TEST_F(LensOverlayCoordinatorTest, ShouldNotShowTheOverlayWhenUIIsNotCreated) {
  // Given a started `LensOverlayCoordinator` without a created UI.
  [coordinator_ start];

  // When the coordinator is asked to show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands) showLensUI:NO];

  // Then nothing should be presented.
  EXPECT_TRUE(base_view_controller_.presentedViewController == nil);
}

// Showing the overlay should present the container view controller.
TEST_F(LensOverlayCoordinatorTest, ShouldPresentVCOnShowCommandDispatched) {
  // Given a started `LensOverlayCoordinator` without a created UI.
  [coordinator_ start];

  // Before showing anything nothing should appear presented.
  EXPECT_TRUE(base_view_controller_.presentedViewController == nil);

  // Dispatch the create & show command.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kLocationBar
               completion:nil];

  // After dispatching the create & show command, a view controller should
  // appear presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Hiding the overlay should trigger dismissing the container VC.
TEST_F(LensOverlayCoordinatorTest, ShouldDismissVCOnHideCommandDispatched) {
  // Given a started `LensOverlayCoordinator` with a created and shown UI.
  [coordinator_ start];

  // Dispatch the create & show command.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kLocationBar
               completion:nil];

  // After dispatching the create & show command, a view controller should
  // appear presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController == nil;
  }));

  [HandlerForProtocol(dispatcher_, LensOverlayCommands) hideLensUI:NO];

  // The presented view controller is set to `nil` when the dismiss is over.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController == nil;
  }));
}

// When the UI is created but not shown, then the memory warning should destroy
// the UI.
TEST_F(LensOverlayCoordinatorTest,
       ShouldDestroyUIOnMemoryWarningWhenUIIsNotShown) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:^(BOOL success) {
                 run_loop_.Quit();
               }];
  run_loop_.Run();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController != nil;
  }));

  // Then the UI should appear created.
  EXPECT_TRUE([coordinator_ isUICreated]);

  // Given a hidden lens overlay.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands) hideLensUI:NO];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController == nil;
  }));

  // When UIKit delivers a low-memory warning notification.
  DeliverMemoryWarningNotification();

  // Then the UI should get destroyed.
  EXPECT_FALSE([coordinator_ isUICreated]);
}

// When the UI is created and visible to the user the memory warning should not
// destroy the UI.
TEST_F(LensOverlayCoordinatorTest,
       ShouldNotDestroyUIOnMemoryWarningWhenUIIsShown) {
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:^(BOOL success) {
                 run_loop_.Quit();
               }];

  run_loop_.Run();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return base_view_controller_.presentedViewController != nil;
  }));

  // Then the UI should appear created and shown to the user.
  EXPECT_TRUE(tab_helper_->IsLensOverlayShown());
  EXPECT_TRUE([coordinator_ isUICreated]);

  // When UIKit delivers a low-memory warning notification.
  DeliverMemoryWarningNotification();

  // Then the UI should not be destroyed.
  EXPECT_TRUE([coordinator_ isUICreated]);
}

// When the user consent have not been received yet, lens coordinator should
// present the consent view controller.
TEST_F(LensOverlayCoordinatorTest, ShouldPresentConsentDialog) {
  profile_->GetPrefs()->SetBoolean(prefs::kLensOverlayConditionsAccepted,
                                   false);

  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:^(BOOL success) {
                 run_loop_.Quit();
               }];

  run_loop_.Run();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return coordinator_.viewController.presentedViewController != nil;
  }));

  UIViewController* presentedVC =
      [coordinator_.viewController presentedViewController];

  EXPECT_TRUE(
      [presentedVC isKindOfClass:[LensOverlayConsentViewController class]]);
}

// When the user consent accepted TOS, lens coordinator shouldn't present the
// consent view controller.
TEST_F(LensOverlayCoordinatorTest, DoesntPromptForConsentWhenAlreadyReceived) {
  profile_->GetPrefs()->SetBoolean(prefs::kLensOverlayConditionsAccepted, true);

  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // When the coordinator is asked to create and show the UI.
  [HandlerForProtocol(dispatcher_, LensOverlayCommands)
      createAndShowLensUI:NO
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:^(BOOL success) {
                 run_loop_.Quit();
               }];

  run_loop_.Run();

  EXPECT_TRUE([coordinator_ isUICreated]);
  UIViewController* presentedVC =
      [coordinator_.viewController presentedViewController];

  EXPECT_FALSE(
      [presentedVC isKindOfClass:[LensOverlayConsentViewController class]]);
}

// Tests that timing metrics are recorded.
TEST_F(LensOverlayCoordinatorTest, TimingMetricsRecorded) {
  base::HistogramTester histogram_tester;
  // Given a started `LensOverlayCoordinator`.
  [coordinator_ start];

  // No metrics should be emitted before anything happens.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionDuration",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionForegroundDuration",
                                    /*expected_count=*/0);

  id<LensOverlayCommands> lens_overlay_handler =
      HandlerForProtocol(dispatcher_, LensOverlayCommands);

  // Create and show lens UI.
  [lens_overlay_handler createAndShowLensUI:NO
                                 entrypoint:LensOverlayEntrypoint::kOverflowMenu
                                 completion:^(BOOL success) {
                                   run_loop_.Quit();
                                 }];

  run_loop_.Run();

  // Destroy Lens UI.
  [lens_overlay_handler destroyLensUI:NO
                               reason:lens::LensOverlayDismissalSource::kOverlayCloseButton];

  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionDuration",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionForegroundDuration",
                                    /*expected_count=*/1);
}

}  // namespace
