// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ActorOverlayCoordinator {
  // The view controller managing the overlay UI.
  ActorOverlayViewController* _viewController;
  // The base color of the overlay UI components.
  UIColor* _overlayColor;
}

#pragma mark - ActorOverlayCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                              overlayColor:(UIColor*)overlayColor {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(overlayColor);
    _overlayColor = overlayColor;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState {
  return [self initWithBaseViewController:viewController
                                  browser:browser
                                 webState:webState
                             overlayColor:[UIColor colorNamed:kBlueColor]];
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (_viewController) {
    return;
  }

  Browser* browser = self.browser;
  LayoutGuideCenter* browserCenter = LayoutGuideCenterForBrowser(browser);

  _viewController = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:browserCenter
                          overlayColor:_overlayColor];

  UIViewController* baseViewController = self.baseViewController;
  [baseViewController addChildViewController:_viewController];
  [baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:baseViewController];
}

- (void)stop {
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
