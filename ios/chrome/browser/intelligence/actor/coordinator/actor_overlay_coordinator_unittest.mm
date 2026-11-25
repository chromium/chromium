// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class ActorOverlayCoordinatorTest : public PlatformTest {
 public:
  ActorOverlayCoordinatorTest(const ActorOverlayCoordinatorTest&) = delete;
  ActorOverlayCoordinatorTest& operator=(const ActorOverlayCoordinatorTest&) =
      delete;

 protected:
  ActorOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    mock_snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
    base_view_controller_ = [[UIViewController alloc] init];
  }

  // Task environment that manages the message loops and threads for testing.
  web::WebTaskEnvironment task_environment_;
  // Profile instance used to initialize the browser.
  std::unique_ptr<TestProfileIOS> profile_;
  // `SceneState` used to attach `LayoutGuideSceneAgent`.
  SceneState* scene_state_ = nil;
  // Browser instance containing the active tab undergoing test.
  std::unique_ptr<TestBrowser> browser_;
  // Strict mock handler for `SnackbarCommands`.
  id mock_snackbar_handler_ = nil;
  // Base view controller used to present the coordinator's UI.
  UIViewController* base_view_controller_ = nil;
};

// Test that `start()` and `stop()` correctly add/remove the child view
// controller and its view.
TEST_F(ActorOverlayCoordinatorTest, StartAndStopLifecycle) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  EXPECT_NE(coordinator, nil);

  // Before starting, no child view controllers.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);

  // Start the coordinator.
  [coordinator start];

  // Verify that an `ActorOverlayViewController` was added as a child.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 1u);
  UIViewController* child =
      base_view_controller_.childViewControllers.firstObject;
  EXPECT_TRUE([child isKindOfClass:[ActorOverlayViewController class]]);

  // Verify that the child's view was added as a subview.
  EXPECT_EQ(child.view.superview, base_view_controller_.view);
  EXPECT_TRUE([base_view_controller_.view.subviews containsObject:child.view]);

  // Stop the coordinator.
  [coordinator stop];

  // Verify everything was cleaned up.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);
  EXPECT_EQ(child.view.superview, nil);
  EXPECT_FALSE([base_view_controller_.view.subviews containsObject:child.view]);
}

// Test that calling `start()` multiple times consecutively does not result in
// duplicate child view controllers or duplicate subviews.
TEST_F(ActorOverlayCoordinatorTest, StartReentrance) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  EXPECT_NE(coordinator, nil);

  // Call `start()` twice.
  [coordinator start];
  [coordinator start];

  // Verify that only a single `ActorOverlayViewController` child and view were
  // added.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 1u);
  UIViewController* child =
      base_view_controller_.childViewControllers.firstObject;
  EXPECT_TRUE([child isKindOfClass:[ActorOverlayViewController class]]);

  EXPECT_EQ(child.view.superview, base_view_controller_.view);
  EXPECT_TRUE([base_view_controller_.view.subviews containsObject:child.view]);

  // Stop the coordinator.
  [coordinator stop];
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);
  EXPECT_EQ(child.view.superview, nil);
  EXPECT_FALSE([base_view_controller_.view.subviews containsObject:child.view]);
}

// Tests that `showInteractionLimitedSnackbar` and scrim taps present the
// snackbar with the expected title and standard duration, deduplicate repeated
// requests until dismissed, and dismiss the snackbar on `stop`.
TEST_F(ActorOverlayCoordinatorTest,
       ShowInteractionLimitedSnackbarDeduplicatesAndDismissesOnStop) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];
  [coordinator start];

  ActorOverlayViewController* overlay_vc =
      base::apple::ObjCCastStrict<ActorOverlayViewController>(
          base_view_controller_.childViewControllers.firstObject);
  ASSERT_NE(overlay_vc, nil);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_ACTOR_OVERLAY_INTERACTION_LIMITED_SNACKBAR);
  __block SnackbarMessage* captured_message = nil;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(SnackbarMessage* msg) {
        captured_message = msg;
        EXPECT_NSEQ(msg.title, expected_title);
        EXPECT_EQ(msg.duration, kSnackbarMessageDuration.InSecondsF());
        EXPECT_EQ(msg.action, nil);
        return YES;
      }]]);

  // First scrim tap presents the snackbar.
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
  ASSERT_NE(captured_message, nil);
  ASSERT_NE(captured_message.completionHandler, nil);

  // Repeated requests while visible are ignored.
  [coordinator showInteractionLimitedSnackbar];
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // Once dismissed via `completionHandler`, a subsequent request presents
  // again.
  captured_message.completionHandler(NO);
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);
  [coordinator showInteractionLimitedSnackbar];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // Stopping the coordinator while visible dismisses the snackbar, clears
  // `overlay_vc.delegate`, and ignores further requests after `stop`.
  OCMExpect([mock_snackbar_handler_ dismissSnackbarWithMessage:expected_title
                                                      animated:YES]);
  [coordinator stop];
  EXPECT_EQ(overlay_vc.delegate, nil);
  [coordinator showInteractionLimitedSnackbar];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

// Tests that `scrimVisible` defaults to `YES`, can be configured to `NO` before
// `-start`, and ignores scrim taps while hidden.
TEST_F(ActorOverlayCoordinatorTest,
       ScrimVisibleDefaultsToYesAndCanBeConfiguredBeforeStart) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];
  EXPECT_TRUE(coordinator.scrimVisible);

  coordinator.scrimVisible = NO;
  [coordinator start];

  ActorOverlayViewController* overlay_vc =
      base::apple::ObjCCastStrict<ActorOverlayViewController>(
          base_view_controller_.childViewControllers.firstObject);
  ASSERT_NE(overlay_vc, nil);
  EXPECT_FALSE(overlay_vc.scrimVisible);

  // Scrim taps and direct snackbar requests are ignored when `scrimVisible` is
  // `NO`.
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  [coordinator showInteractionLimitedSnackbar];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  [coordinator stop];
}

// Tests that toggling `scrimVisible` updates the view controller, dismisses any
// presented interaction-limited snackbar when set to `NO`, and re-enables scrim
// tap handling when set back to `YES`.
TEST_F(ActorOverlayCoordinatorTest,
       SetScrimVisibleUpdatesViewControllerAndDismissesSnackbar) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];
  [coordinator start];

  ActorOverlayViewController* overlay_vc =
      base::apple::ObjCCastStrict<ActorOverlayViewController>(
          base_view_controller_.childViewControllers.firstObject);
  ASSERT_NE(overlay_vc, nil);
  EXPECT_TRUE(overlay_vc.scrimVisible);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_ACTOR_OVERLAY_INTERACTION_LIMITED_SNACKBAR);
  __block SnackbarMessage* first_message = nil;
  OCMExpect([mock_snackbar_handler_
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(SnackbarMessage* msg) {
        first_message = msg;
        return YES;
      }]]);
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
  ASSERT_NE(first_message, nil);

  // Hiding the scrim forwards the state and dismisses the active snackbar.
  OCMExpect([mock_snackbar_handler_ dismissSnackbarWithMessage:expected_title
                                                      animated:YES]);
  coordinator.scrimVisible = NO;
  EXPECT_FALSE(overlay_vc.scrimVisible);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // Tapping while hidden is a no-op.
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // Showing the scrim again re-enables snackbar presentation on tap.
  coordinator.scrimVisible = YES;
  EXPECT_TRUE(overlay_vc.scrimVisible);
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // A delayed completion from the previously dismissed snackbar must not clear
  // the newly presented snackbar.
  first_message.completionHandler(NO);
  [overlay_vc.delegate actorOverlayViewControllerDidTapScrim:overlay_vc];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  OCMExpect([mock_snackbar_handler_ dismissSnackbarWithMessage:expected_title
                                                      animated:YES]);
  [coordinator stop];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

}  // namespace
