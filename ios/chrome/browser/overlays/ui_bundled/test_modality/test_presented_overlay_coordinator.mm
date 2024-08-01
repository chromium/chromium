// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_presented_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface TestPresentedOverlayCoordinator ()
// Redefine as readwrite.
@property(nonatomic, readwrite) UIViewController* presentedViewController;
@end

@implementation TestPresentedOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return TestPresentedOverlay::RequestSupport();
}

- (UIViewController*)viewController {
  return self.presentedViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started)
    return;
  self.presentedViewController = [[UIViewController alloc] init];
  self.viewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.baseViewController.definesPresentationContext = YES;
  [self.baseViewController
      presentViewController:self.viewController
                   animated:animated
                 completion:^{
                   self.delegate->OverlayUIDidFinishPresentation(self.request);
                 }];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  [self.baseViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.delegate->OverlayUIDidFinishDismissal(
                               self.request);
                         }];
  self.presentedViewController = nil;
  self.started = NO;
}

@end
