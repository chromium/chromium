// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/chrome_activity_overlay_coordinator.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/elements/chrome_activity_overlay_view_controller.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ChromeActivityOverlayCoordinator ()
// View controller that displays a native active indicator.
@property(nonatomic, strong)
    ChromeActivityOverlayViewController* chromeActivityOverlayViewController;
@end

@implementation ChromeActivityOverlayCoordinator {
  std::unique_ptr<ScopedUIBlocker> _windowUIBlocker;
}

- (void)start {
  if (self.chromeActivityOverlayViewController || self.started) {
    return;
  }

  self.chromeActivityOverlayViewController =
      [[ChromeActivityOverlayViewController alloc] init];

  self.chromeActivityOverlayViewController.messageText = self.messageText;
  [self.baseViewController
      addChildViewController:self.chromeActivityOverlayViewController];
  // Make sure frame of view is exactly the same size as its presenting view.
  // Especially important when the presenting view is a bubble.
  CGRect frame = self.chromeActivityOverlayViewController.view.frame;
  frame.origin.x = 0;
  frame.size.width = self.baseViewController.view.bounds.size.width;
  frame.size.height = self.baseViewController.view.bounds.size.height;
  self.chromeActivityOverlayViewController.view.frame = frame;
  [self.baseViewController.view
      addSubview:self.chromeActivityOverlayViewController.view];
  [self.chromeActivityOverlayViewController
      didMoveToParentViewController:self.baseViewController];

  if (self.blockAllWindows) {
    SceneState* sceneState = self.browser->GetSceneState();
    _windowUIBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  }

  self.started = YES;
}

- (void)stop {
  if (!self.chromeActivityOverlayViewController || !self.started) {
    return;
  }
  _windowUIBlocker.reset();
  [self.chromeActivityOverlayViewController willMoveToParentViewController:nil];
  [self.chromeActivityOverlayViewController.view removeFromSuperview];
  [self.chromeActivityOverlayViewController removeFromParentViewController];
  self.chromeActivityOverlayViewController = nil;
  self.started = NO;
}

@end
