// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_coordinator.h"

#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_view_controller.h"

@interface OmniboxPositionChoiceCoordinator () <
    PromoStyleViewControllerDelegate>

@end

@implementation OmniboxPositionChoiceCoordinator {
  /// View controller of the omnibox position choice screen.
  OmniboxPositionChoiceViewController* _viewController;
  /// Whether the screen is being shown in the FRE.
  BOOL _firstRun;
  /// First run screen delegate.
  __weak id<FirstRunScreenDelegate> _first_run_delegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _firstRun = NO;
  }
  return self;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [self initWithBaseViewController:navigationController browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = YES;
    _first_run_delegate = delegate;
  }
  return self;
}

- (void)start {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  [super start];

  _viewController = [[OmniboxPositionChoiceViewController alloc] init];
  _viewController.delegate = self;
  _viewController.modalInPresentation = YES;

  if (_firstRun) {
    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ _viewController ]
                                             animated:animated];
    // TODO(crbug.com/1503638): Record metric here.
  } else {
    [self.baseViewController presentViewController:_viewController
                                          animated:YES
                                        completion:nil];
    // TODO(crbug.com/1503638): Record metric here.
  }
}

- (void)stop {
  if (!_firstRun) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _viewController = nil;
  _baseNavigationController = nil;
  _first_run_delegate = nil;
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1503638): Implement primary action.
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/1503638): Implement secondary action.
  [self dismissScreen];
}

#pragma mark - Private

- (void)dismissScreen {
  if (_firstRun) {
    [_first_run_delegate screenWillFinishPresenting];
  } else {
    // TODO(crbug.com/1503638): Implement browser dismissal here.
  }
}

@end
