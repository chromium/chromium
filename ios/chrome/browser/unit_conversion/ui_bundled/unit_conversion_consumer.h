// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSUMER_H_

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

// UnitConversionConsumer defines methods to set the contents of the
// UnitConversionViewController.
@protocol UnitConversionConsumer <NSObject>

// Tells the consumer to update the unit type title (volume, mass, etc) based on
// the user's new selected unit type.
- (void)updateUnitTypeTitle:(ios::provider::UnitType)unitTypeTitle;

// Tells the consumer to update the source unit based on the user's new selected
// source unit type, and reload its source unit related fields.
- (void)updateSourceUnit:(NSUnit*)sourceUnit reload:(BOOL)reload;

// Tells the consumer to update the target unit based on the user's new selected
// target unit type and reload its target unit related fields.
- (void)updateTargetUnit:(NSUnit*)targetUnit reload:(BOOL)reload;

// Tells the consumer to update the source unit value and reload its source unit
// value related fields.
- (void)updateSourceUnitValue:(double)sourceUnitValue reload:(BOOL)reload;

// Tells the consumer to update the target unit value and reload its target unit
// value related fields.
- (void)updateTargetUnitValue:(double)targetUnitValue reload:(BOOL)reload;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSUMER_H_
