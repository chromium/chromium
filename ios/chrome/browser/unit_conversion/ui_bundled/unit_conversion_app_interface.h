// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// EG test app interface managing the unit conversion feature.
@interface UnitConversionAppInterface : NSObject

// Presents the unit conversion view controller using the
// `UnitConversionCommands` of the current Browser.
+ (void)presentUnitConversionFeature;

// Stops presenting the unit conversion view controller using the
// `UnitConversionCommands` of the current Browser.
+ (void)stopPresentingUnitConversionFeature;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_APP_INTERFACE_H_
