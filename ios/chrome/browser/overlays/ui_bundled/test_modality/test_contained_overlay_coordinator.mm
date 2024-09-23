// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_contained_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/test_modality/test_contained_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface TestContainedOverlayCoordinator ()
@property(nonatomic, readwrite) UIViewController* containedViewController;
@end

@implementation TestContainedOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return TestContainedOverlay::RequestSupport();
}

+ (BOOL)showsOverlayUsingChildViewController {
  return YES;
}

- (UIViewController*)viewController {
  return self.containedViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started)
    return;
  self.containedViewController = [[UIViewController alloc] init];
  UIView* view = self.viewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:self.viewController];
  [self.baseViewController.view addSubview:view];
  AddSameConstraints(view, view.superview);
  [self.viewController didMoveToParentViewController:self.baseViewController];
  self.delegate->OverlayUIDidFinishPresentation(self.request);
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  [self.viewController willMoveToParentViewController:nil];
  [self.viewController.view removeFromSuperview];
  [self.viewController removeFromParentViewController];
  self.containedViewController = nil;
  self.started = NO;
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

@end
