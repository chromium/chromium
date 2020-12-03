// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/browser/ui/blocking_overlay/blocking_overlay_view_controller.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UIBlockerSceneAgent ()

// TODO(crbug.com/1107873): Create a coordinator to own this view controller.
// The view controller that blocks all interactions with the scene.
@property(nonatomic, strong)
    BlockingOverlayViewController* blockingOverlayViewController;

@end

@implementation UIBlockerSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneStateWillShowModalOverlay:(SceneState*)sceneState {
  [self displayBlockingOverlay];
}

- (void)sceneStateWillHideModalOverlay:(SceneState*)sceneState {
  if (!self.blockingOverlayViewController) {
    return;
  }

  [self.blockingOverlayViewController.view removeFromSuperview];
  self.blockingOverlayViewController = nil;

  // When the scene has displayed the blocking overlay and isn't in foreground
  // when it exits it, the cached app switcher snapshot will have the overlay on
  // it, and therefore needs updating.
  if (sceneState.activationLevel < SceneActivationLevelForegroundInactive) {
    if (@available(iOS 13, *)) {
      if (IsMultiwindowSupported()) {
        DCHECK(sceneState.scene.session);
        [[UIApplication sharedApplication]
            requestSceneSessionRefresh:sceneState.scene.session];
      }
    }
  }
}

#pragma mark - private

- (void)displayBlockingOverlay {
  if (self.blockingOverlayViewController) {
    // The overlay is already displayed, nothing to do.
    return;
  }

  // Make the window visible. This is because in safe mode it's not visible yet.
  if (self.sceneState.window.hidden) {
    [self.sceneState.window makeKeyAndVisible];
  }

  self.blockingOverlayViewController =
      [[BlockingOverlayViewController alloc] init];
  self.blockingOverlayViewController.blockingSceneCommandHandler =
      HandlerForProtocol(self.sceneState.appState.appCommandDispatcher,
                         BlockingSceneCommands);
  UIView* overlayView = self.blockingOverlayViewController.view;
  [self.sceneState.window addSubview:overlayView];
  overlayView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.sceneState.window, overlayView);
}

@end
