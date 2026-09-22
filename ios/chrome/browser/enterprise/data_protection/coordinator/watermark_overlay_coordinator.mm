// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_overlay_coordinator.h"

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_overlay_mediator.h"
#import "ios/chrome/browser/enterprise/data_protection/model/watermark_request_config.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_overlay_view_controller.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation WatermarkOverlayCoordinator {
  // The view controller used to display watemrark.
  WatermarkOverlayViewController* _watermarkViewController;
  // The mediator used to display watermark.
  WatermarkOverlayMediator* _watermarkMediator;
}

#pragma mark - OverlayRequestCoordinator

+ (BOOL)showsOverlayUsingChildViewController {
  return YES;
}

+ (const OverlayRequestSupport*)requestSupport {
  return WatermarkRequestConfig::RequestSupport();
}

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request) {
    return;
  }

  _watermarkViewController = [[WatermarkOverlayViewController alloc] init];
  _watermarkMediator = [[WatermarkOverlayMediator alloc]
      initWithRequest:self.request
          prefService:self.browser->GetProfile()->GetPrefs()];
  _watermarkMediator.consumer = _watermarkViewController;

  // Add the watermark container view controller to the hierarchy.
  UIView* view = _watermarkViewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:_watermarkViewController];
  [self.baseViewController.view addSubview:view];
  AddSameConstraints(view, view.superview);
  [_watermarkViewController
      didMoveToParentViewController:self.baseViewController];
  self.started = YES;
  self.delegate->OverlayUIDidFinishPresentation(self.request);
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started) {
    return;
  }

  [_watermarkViewController willMoveToParentViewController:nil];
  [_watermarkViewController.view removeFromSuperview];
  [_watermarkViewController removeFromParentViewController];

  [_watermarkMediator disconnect];
  _watermarkMediator = nil;
  self.started = NO;
  _watermarkViewController = nil;

  // Notify delegate that dismissal finished immediately.
  self.delegate->OverlayUIDidFinishDismissal(self.requestId);
}

- (UIViewController*)viewController {
  return _watermarkViewController;
}

@end
