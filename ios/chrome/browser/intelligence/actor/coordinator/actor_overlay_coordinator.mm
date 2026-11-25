// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ActorOverlayCoordinator () <ActorOverlayViewControllerDelegate>
@end

@implementation ActorOverlayCoordinator {
  // The view controller managing the overlay UI.
  ActorOverlayViewController* _viewController;
  // Handler used to present and dismiss snackbars.
  id<SnackbarCommands> _snackbarCommandsHandler;
  // Currently presented snackbar informing the user that page interaction is
  // limited while the actor controls the tab, or `nil` if none is visible.
  SnackbarMessage* _interactionLimitedSnackbar;
}

#pragma mark - Public

- (void)showInteractionLimitedSnackbar {
  // No-op before `start`, after `stop`, when the scrim is hidden, or while the
  // snackbar is already visible.
  if (!_viewController || !self.scrimVisible || _interactionLimitedSnackbar) {
    return;
  }

  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_ACTOR_OVERLAY_INTERACTION_LIMITED_SNACKBAR)];
  _interactionLimitedSnackbar = message;
  __weak __typeof(self) weakSelf = self;
  __weak SnackbarMessage* weakMessage = message;
  message.completionHandler = ^(BOOL) {
    [weakSelf clearInteractionLimitedSnackbarForMessage:weakMessage];
  };

  [_snackbarCommandsHandler showSnackbarMessage:message];
}

#pragma mark - Properties

- (void)setScrimVisible:(BOOL)scrimVisible {
  if (_scrimVisible == scrimVisible) {
    return;
  }
  _scrimVisible = scrimVisible;
  _viewController.scrimVisible = _scrimVisible;
  if (!_scrimVisible) {
    [self dismissInteractionLimitedSnackbar];
  }
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _scrimVisible = YES;
  }
  return self;
}

- (void)start {
  if (_viewController) {
    return;
  }

  Browser* browser = self.browser;
  LayoutGuideCenter* browserCenter = LayoutGuideCenterForBrowser(browser);
  SceneLayoutState* layoutState = browser->GetSceneState().layoutState;
  _snackbarCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);

  UIColor* overlayColor = [UIColor colorNamed:kStaticBlueColor];
  _viewController = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:browserCenter
                            scrimColor:overlayColor
                             glowColor:overlayColor];
  _viewController.delegate = self;
  _viewController.scrimVisible = self.scrimVisible;
  _viewController.layoutState = layoutState;

  UIViewController* baseViewController = self.baseViewController;
  [baseViewController addChildViewController:_viewController];
  [baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:baseViewController];
}

- (void)stop {
  [self dismissInteractionLimitedSnackbar];
  _snackbarCommandsHandler = nil;
  _viewController.delegate = nil;
  _viewController.layoutState = nil;
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

#pragma mark - ActorOverlayViewControllerDelegate

- (void)actorOverlayViewControllerDidTapScrim:
    (ActorOverlayViewController*)viewController {
  CHECK_EQ(_viewController, viewController);
  [self showInteractionLimitedSnackbar];
}

#pragma mark - Private

// Dismisses `_interactionLimitedSnackbar` if one is currently presented.
- (void)dismissInteractionLimitedSnackbar {
  if (!_interactionLimitedSnackbar) {
    return;
  }
  [_snackbarCommandsHandler
      dismissSnackbarWithMessage:_interactionLimitedSnackbar.title
                        animated:YES];
  _interactionLimitedSnackbar = nil;
}

// Clears `_interactionLimitedSnackbar` when `message` finishes dismissing, if
// it is still the active snackbar.
- (void)clearInteractionLimitedSnackbarForMessage:(SnackbarMessage*)message {
  if (_interactionLimitedSnackbar == message) {
    _interactionLimitedSnackbar = nil;
  }
}

@end
