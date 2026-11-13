// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/signin_modal_host.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_sign_in_commands.h"
#import "ios/web/public/test/web_task_environment.h"
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
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
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
  std::unique_ptr<TestProfileIOS> profile_;
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
