// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/signin_modal_host.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>
#import <utility>

#import "base/functional/bind.h"
#import "base/types/expected.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_sign_in_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns the target handling the `SceneSignInCommands` sent through
// `browser`'s dispatcher.
id SignInHandler(Browser* browser) {
  return [browser->GetCommandDispatcher()
      forwardingTargetForSelector:
          @selector(stopSigninCoordinatorWithCompletionAnimated:)];
}

}  // namespace

// Test fixture for SigninModalHost testing.
class SigninModalHostTest : public PlatformTest {
 protected:
  SigninModalHostTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();
    scene_state_ = [[SceneState alloc] init];
    scene_state_.profileState = profile_state_;
    [profile_state_ sceneStateConnected:scene_state_];
    regular_browser_ =
        std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    incognito_browser_ = std::make_unique<TestBrowser>(
        profile_->CreateOffTheRecordProfileWithTestingFactories(
            TestProfileIOS::TestingFactories()),
        scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
    signin_modal_host_ = [[SigninModalHost alloc]
        initWithRegularBrowser:regular_browser_.get()
              incognitoBrowser:incognito_browser_.get()];
    [signin_modal_host_ setBaseViewControllerForModals:base_view_controller_];
    [signin_modal_host_ startHostingCommandProtocols];
  }

  ~SigninModalHostTest() override {
    [signin_modal_host_ stopHostingCommandProtocols];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  UIViewController* base_view_controller_;
  SigninModalHost* signin_modal_host_;
};

// Tests that `SigninModalHost` handles `SceneSignInCommands` for its browsers,
// follows the reconnection of the incognito browser, and stops cleanly.
TEST_F(SigninModalHostTest, TestInitializeAndStop) {
  EXPECT_NE(signin_modal_host_, nil);
  EXPECT_EQ(SignInHandler(regular_browser_.get()), signin_modal_host_);
  EXPECT_EQ(SignInHandler(incognito_browser_.get()), signin_modal_host_);

  [signin_modal_host_ disconnectIncognitoBrowser];
  EXPECT_FALSE([incognito_browser_->GetCommandDispatcher()
      dispatchingForProtocol:@protocol(SceneSignInCommands)]);

  [signin_modal_host_ connectIncognitoBrowser:incognito_browser_.get()];
  EXPECT_EQ(SignInHandler(incognito_browser_.get()), signin_modal_host_);

  // Stopping the sign-in flow when none is presented is a no-op.
  [signin_modal_host_ stopSigninCoordinatorWithCompletionAnimated:NO];
  [signin_modal_host_ clearPresentedStateAnimated:NO];

  [signin_modal_host_ stopHostingCommandProtocols];
  EXPECT_FALSE([regular_browser_->GetCommandDispatcher()
      dispatchingForProtocol:@protocol(SceneSignInCommands)]);
  EXPECT_FALSE([incognito_browser_->GetCommandDispatcher()
      dispatchingForProtocol:@protocol(SceneSignInCommands)]);
}

// Tests that `-showAccountSwitchingPromoWithIdentity:completion:` interrupts
// cleanly without crashing when another scene on the same profile is already
// blocking the UI.
TEST_F(SigninModalHostTest, ShowAccountSwitchingPromoInterruptedWhenUIBlocked) {
  ASSERT_TRUE(AuthenticationServiceFactory::GetForProfile(profile_.get())
                  ->SigninEnabled());
  // Connect a second scene to the same profile and acquire a profile-scoped
  // sign-in UI blocker on it so `scene_state_` becomes UI-blocked.
  SceneState* other_scene_state = [[SceneState alloc] init];
  other_scene_state.profileState = profile_state_;
  [profile_state_ sceneStateConnected:other_scene_state];
  std::unique_ptr<SigninInProgress> other_signin_in_progress =
      [other_scene_state createSigninInProgress];
  ASSERT_TRUE(scene_state_.isUIBlocked);

  __block std::optional<SigninCoordinatorResultOrIdentity> completion_result;
  // Request the account switching promo on the blocked scene.
  [signin_modal_host_
      showAccountSwitchingPromoWithIdentity:[FakeSystemIdentity fakeIdentity1]
                                 completion:^(
                                     SigninCoordinator* coordinator,
                                     SigninCoordinatorResultOrIdentity result) {
                                   EXPECT_EQ(coordinator, nil);
                                   completion_result = std::move(result);
                                 }];
  ASSERT_TRUE(completion_result.has_value());
  ASSERT_FALSE(completion_result->has_value());
  EXPECT_EQ(completion_result->error(), SigninCoordinatorResultInterrupted);

  other_signin_in_progress.reset();
}

// Tests that `-showAccountSwitchingPromoWithIdentity:completion:` invokes the
// completion with `SigninCoordinatorResultDisabled` and a `nil` coordinator
// when sign-in is disabled.
TEST_F(SigninModalHostTest,
       ShowAccountSwitchingPromoDisabledWhenSigninDisabled) {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice, false);
  ASSERT_FALSE(AuthenticationServiceFactory::GetForProfile(profile_.get())
                   ->SigninEnabled());

  __block std::optional<SigninCoordinatorResultOrIdentity> completion_result;
  [signin_modal_host_
      showAccountSwitchingPromoWithIdentity:[FakeSystemIdentity fakeIdentity1]
                                 completion:^(
                                     SigninCoordinator* coordinator,
                                     SigninCoordinatorResultOrIdentity result) {
                                   EXPECT_EQ(coordinator, nil);
                                   completion_result = std::move(result);
                                 }];
  ASSERT_TRUE(completion_result.has_value());
  ASSERT_FALSE(completion_result->has_value());
  EXPECT_EQ(completion_result->error(), SigninCoordinatorResultDisabled);
}
