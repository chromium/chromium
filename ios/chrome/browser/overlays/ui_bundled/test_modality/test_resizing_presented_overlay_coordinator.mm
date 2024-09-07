// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_resizing_presented_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_resizing_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#pragma mark - FakeResizingPresentationController

@interface FakeResizingPresentationController : OverlayPresentationController
// The frame of the presentation container view in window coordinates.
@property(nonatomic, readonly) CGRect windowFrame;
// Initializer for a presentation controller that lays its presentation
// container view with `windowFrame`.
- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                        windowFrame:(CGRect)windowFrame
    NS_DESIGNATED_INITIALIZER;
- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;
@end

@implementation FakeResizingPresentationController

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                        windowFrame:(CGRect)windowFrame {
  if ((self =
           [super initWithPresentedViewController:presentedViewController
                         presentingViewController:presentingViewController])) {
    _windowFrame = windowFrame;
  }
  return self;
}

#pragma mark OverlayPresentationController

- (BOOL)resizesPresentationContainer {
  return YES;
}

#pragma mark UIPresentationController

- (void)presentationTransitionWillBegin {
  [self updateLayout];
}

- (void)containerViewWillLayoutSubviews {
  [self updateLayout];
  [super containerViewWillLayoutSubviews];
}

// Lays out the container view according to the window frame.
- (void)updateLayout {
  self.containerView.frame =
      [self.containerView.superview convertRect:self.windowFrame fromView:nil];
  self.presentedView.frame = self.containerView.bounds;
}

@end

#pragma mark - TestResizingPresentedOverlayCoordinator

@interface TestResizingPresentedOverlayCoordinator () <
    UIViewControllerTransitioningDelegate>
@property(nonatomic, readwrite) UIViewController* presentedViewController;
@end

@implementation TestResizingPresentedOverlayCoordinator

#pragma mark OverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return TestResizingPresentedOverlay::RequestSupport();
}

- (UIViewController*)viewController {
  return self.presentedViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started)
    return;
  self.presentedViewController = [[UIViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationCustom;
  self.viewController.transitioningDelegate = self;
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

#pragma mark UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  CGRect windowFrame =
      self.request->GetConfig<TestResizingPresentedOverlay>()->frame();
  return [[FakeResizingPresentationController alloc]
      initWithPresentedViewController:presented
             presentingViewController:presenting
                          windowFrame:windowFrame];
}

@end
