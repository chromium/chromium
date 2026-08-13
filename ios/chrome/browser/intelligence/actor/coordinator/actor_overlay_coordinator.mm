// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ActorOverlayCoordinator {
  // The view controller managing the overlay UI.
  ActorOverlayViewController* _viewController;
  // The base color of the scrim.
  UIColor* _scrimColor;
  // The base color of the glow.
  UIColor* _glowColor;
}

#pragma mark - ActorOverlayCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                scrimColor:(UIColor*)scrimColor
                                 glowColor:(UIColor*)glowColor {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(scrimColor);
    CHECK(glowColor);
    _scrimColor = scrimColor;
    _glowColor = glowColor;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState {
  return [self initWithBaseViewController:viewController
                                  browser:browser
                                 webState:webState
                               scrimColor:[UIColor colorNamed:kBlueColor]
                                glowColor:[UIColor colorNamed:kBlue900Color]];
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (_viewController) {
    return;
  }

  Browser* browser = self.browser;
  LayoutGuideCenter* browserCenter = LayoutGuideCenterForBrowser(browser);
  SceneLayoutState* layoutState =
      browser ? browser->GetSceneState().layoutState : nil;

  _viewController = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:browserCenter
                            scrimColor:_scrimColor
                             glowColor:_glowColor];
  _viewController.layoutState = layoutState;

  UIViewController* baseViewController = self.baseViewController;
  [baseViewController addChildViewController:_viewController];
  [baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:baseViewController];
}

- (void)stop {
  _viewController.layoutState = nil;
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
