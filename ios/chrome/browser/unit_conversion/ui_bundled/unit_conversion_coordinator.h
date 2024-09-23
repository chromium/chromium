// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_view_controller_delegate.h"

// This coordinator presents unit conversion sheet to the user.
@interface UnitConversionCoordinator
    : ChromeCoordinator <UnitConversionViewControllerDelegate>

// Init UnitConversionCoordinator with the detected unit `sourceUnit`, the
// detected unit value `sourceUnitValue` and the tap/long press location
// `location`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                sourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_COORDINATOR_H_
