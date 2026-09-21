// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/web_content_area/spinning_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Duration to show or hide the overlay.
constexpr NSTimeInterval kOverlayViewAnimationDuration = 0.3;

}  // namespace

@implementation SpinningOverlayCoordinator {
  // The view controller that displays the overlay.
  UIViewController* _containerViewController;
  // The spinning overlay view.
  SpinningOverlayView* _overlayView;
}

#pragma mark - OverlayRequestCoordinator

- (void)stop {
  [super stop];

  // Ensure view container is removed if coordinator is destroyed before the
  // animation finishes in `stopAnimated`.
  if (_containerViewController) {
    [self finishDismissal];
  }
}

+ (const OverlayRequestSupport*)requestSupport {
  return SpinningOverlayRequestConfig::RequestSupport();
}

+ (BOOL)showsOverlayUsingChildViewController {
  return YES;
}

- (UIViewController*)viewController {
  return _containerViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started) {
    return;
  }

  SpinningOverlayRequestConfig* config =
      self.request ? self.request->GetConfig<SpinningOverlayRequestConfig>()
                   : nullptr;
  NSString* labelText = config ? config->label_text() : nil;
  BOOL cancellable = config ? config->is_cancellable() : NO;
  _overlayView = [[SpinningOverlayView alloc] initWithLabelText:labelText
                                                    cancellable:cancellable];

  SpinningOverlayMediator* spinningOverlayMediator =
      [[SpinningOverlayMediator alloc] initWithRequest:self.request];
  self.mediator = spinningOverlayMediator;
  _overlayView.delegate = spinningOverlayMediator;

  _containerViewController = [[UIViewController alloc] init];
  _containerViewController.view = _overlayView;

  [self.baseViewController addChildViewController:_containerViewController];
  [self.baseViewController.view addSubview:_containerViewController.view];
  _containerViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(_containerViewController.view,
                     self.baseViewController.view);
  [_containerViewController
      didMoveToParentViewController:self.baseViewController];

  if (!animated) {
    _containerViewController.view.alpha = 1.0;
    [self finishPresentation];
    self.started = YES;
    return;
  }

  _containerViewController.view.alpha = 0.0;
  UIViewController* containerViewController = _containerViewController;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
      animations:^{
        containerViewController.view.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        [weakSelf finishPresentation];
      }];

  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started) {
    return;
  }
  self.started = NO;

  [self.mediator disconnect];
  self.mediator = nil;
  _overlayView = nil;

  if (!animated) {
    _containerViewController.view.alpha = 0.0;
    [self finishDismissal];
    return;
  }

  UIViewController* containerViewController = _containerViewController;
  __weak SpinningOverlayCoordinator* weakSelf = self;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
      animations:^{
        containerViewController.view.alpha = 0.0;
      }
      completion:^(BOOL finished) {
        [weakSelf finishDismissal];
      }];
}

#pragma mark - Private

// Notifies the delegate that presentation finished.
- (void)finishPresentation {
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishPresentation(self.request);
  }
}

// Notifies the delegate that dismissal finished.
- (void)finishDismissal {
  if (!_containerViewController) {
    return;
  }
  [_containerViewController willMoveToParentViewController:nil];
  [_containerViewController.view removeFromSuperview];
  [_containerViewController removeFromParentViewController];
  _containerViewController = nil;
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishDismissal(self.requestId);
  }
}

@end
