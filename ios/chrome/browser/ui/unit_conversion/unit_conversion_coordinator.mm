// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_coordinator.h"

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mediator.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_view_controller.h"

namespace {

// Popover source rect dimensions.
const CGFloat kPopOverSourceRectWidth = 1;
const CGFloat kPopOverSourceRectHeight = 1;

}  // namespace

@interface UnitConversionCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation UnitConversionCoordinator {
  // The view controller managed by this coordinator.
  UnitConversionViewController* _viewController;

  // Mediator to handle the units updates and conversion.
  UnitConversionMediator* _mediator;

  // The detected unit.
  NSUnit* _sourceUnit;

  // The detected unit value.
  double _sourceUnitValue;

  // The user's tap/long press location.
  CGPoint _location;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                sourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _sourceUnit = sourceUnit;
    _sourceUnitValue = sourceUnitValue;
    _location = location;
  }
  return self;
}

- (void)start {
  _viewController = [[UnitConversionViewController alloc]
      initWithSourceUnit:_sourceUnit
               unitValue:_sourceUnitValue];

  _mediator = [[UnitConversionMediator alloc] init];

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  [self presentUnitConversionViewController];
}

- (void)stop {
  [self dismissUnitConversionViewController];
}

#pragma mark - Private

// Presents the UnitConversionCoordinator's view controller and adapt the
// presentation based on the device (popover for ipad, half sheet for iphone)

- (void)presentUnitConversionViewController {
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationPopover;
  UIPopoverPresentationController* popover =
      navigationController.popoverPresentationController;
  popover.delegate = _viewController;
  popover.sourceView = self.baseViewController.view;
  popover.sourceRect =
      CGRectMake(_location.x, _location.y, kPopOverSourceRectWidth,
                 kPopOverSourceRectHeight);
  popover.permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;
  UISheetPresentationController* sheetPresentationController =
      popover.adaptiveSheetPresentationController;
  sheetPresentationController.delegate = _viewController;
  sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
  sheetPresentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Dismisses the UnitConversionCoordinator's view controller.
- (void)dismissUnitConversionViewController {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

@end
