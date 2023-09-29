// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_coordinator.h"

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mediator.h"

@interface UnitConversionCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation UnitConversionCoordinator {
  // TODO(crbug.com/1468905): Add the unit conversion view controller.

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
  _mediator = [[UnitConversionMediator alloc] init];
  [self presentUnitConversionViewController];
}

- (void)stop {
  [self dismissUnitConversionViewController];
}

#pragma mark - Private

// Presents the UnitConversionCoordinator's view controller and adapt the
// presentation based on the device (popover for ipad, half sheet for iphone)
- (void)presentUnitConversionViewController {
  // TODO(crbug.com/1468905): Present the unit conversion view controller.
  return;
}

// Dismisses the UnitConversionCoordinator's view controller.
- (void)dismissUnitConversionViewController {
  // TODO(crbug.com/1468905): Dismiss the unit conversion view controller.
  return;
}

@end
