// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

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
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
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

}  // namespace
