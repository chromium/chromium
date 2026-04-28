// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/ui_bundled/ui_blocker_scene_agent.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/blocking_overlay/ui_bundled/blocking_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_ui_blocker_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@interface UIBlockerSceneAgent () <SceneUIBlockerStateObserver>

@property(nonatomic, strong) UIWindow* overlayWindow;

@end

@implementation UIBlockerSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [sceneState.uiBlockerState addObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [sceneState.uiBlockerState removeObserver:self];
}

#pragma mark - SceneUIBlockerStateObserver

- (void)willShowModalOverlay {
  [self displayBlockingOverlay];
}

- (void)willHideModalOverlay {
  if (!self.overlayWindow) {
    return;
  }

  self.overlayWindow = nil;

  // When the scene has displayed the blocking overlay and isn't in foreground
  // when it exits it, the cached app switcher snapshot will have the overlay on
  // it, and therefore needs updating.
  if (self.sceneState.activationLevel <
      SceneActivationLevelForegroundInactive) {
    DCHECK(self.sceneState.scene.session);
    [[UIApplication sharedApplication]
        requestSceneSessionRefresh:self.sceneState.scene.session];
  }
}

#pragma mark - private

- (void)displayBlockingOverlay {
  if (self.overlayWindow) {
    // The overlay is already displayed, nothing to do.
    return;
  }

  self.overlayWindow =
      [[UIWindow alloc] initWithWindowScene:self.sceneState.scene];

  // The blocker is above everything, including the alerts, but below the status
  // bar.
  self.overlayWindow.windowLevel = UIWindowLevelStatusBar - 1;
  NSString* a11yIdentifier = [@"blocker-"
      stringByAppendingString:self.sceneState.window.accessibilityIdentifier];
  self.overlayWindow.accessibilityIdentifier = a11yIdentifier;

  // TODO(crbug.com/40707167): Create a coordinator to own this view controller.
  // The view controller that blocks all interactions with the scene.
  BlockingOverlayViewController* blockingOverlayViewController =

      [[BlockingOverlayViewController alloc] init];
  blockingOverlayViewController.blockingSceneCommandHandler =
      HandlerForProtocol(
          self.sceneState.profileState.appState.appCommandDispatcher,
          BlockingSceneCommands);

  self.overlayWindow.rootViewController = blockingOverlayViewController;
  [self.overlayWindow makeKeyAndVisible];
}

@end
