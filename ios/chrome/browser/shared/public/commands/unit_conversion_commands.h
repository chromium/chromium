// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_UNIT_CONVERSION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_UNIT_CONVERSION_COMMANDS_H_

// Commands related to the Unit Conversion feature.
@protocol UnitConversionCommands <NSObject>

// Shows unit conversion for a source unit/value/location (location of the tap
// in the web view). Note that the set of units is not restricted.
- (void)presentUnitConversionForSourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location;

// Hides the unit conversion view.
- (void)hideUnitConversion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_UNIT_CONVERSION_COMMANDS_H_
