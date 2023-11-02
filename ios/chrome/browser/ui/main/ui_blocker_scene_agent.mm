// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/browser/ui/blocking_overlay/blocking_overlay_view_controller.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UIBlockerSceneAgent ()

@property(nonatomic, strong) UIWindow* overlayWindow;

@end

@implementation UIBlockerSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneStateWillShowModalOverlay:(SceneState*)sceneState {
  [self displayBlockingOverlay];
}

- (void)sceneStateWillHideModalOverlay:(SceneState*)sceneState {
  if (!self.overlayWindow) {
    return;
  }

  self.overlayWindow = nil;

  // When the scene has displayed the blocking overlay and isn't in foreground
  // when it exits it, the cached app switcher snapshot will have the overlay on
  // it, and therefore needs updating.
  if (sceneState.activationLevel < SceneActivationLevelForegroundInactive) {
    DCHECK(sceneState.scene.session);
    [[UIApplication sharedApplication]
        requestSceneSessionRefresh:sceneState.scene.session];
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

  // TODO(crbug.com/1107873): Create a coordinator to own this view controller.
  // The view controller that blocks all interactions with the scene.
  BlockingOverlayViewController* blockingOverlayViewController =

      [[BlockingOverlayViewController alloc] init];
  blockingOverlayViewController.blockingSceneCommandHandler =
      HandlerForProtocol(self.sceneState.appState.appCommandDispatcher,
                         BlockingSceneCommands);

  self.overlayWindow.rootViewController = blockingOverlayViewController;
  [self.overlayWindow makeKeyAndVisible];
}

@end
