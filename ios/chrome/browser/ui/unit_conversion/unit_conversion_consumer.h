// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_CONSUMER_H_

// UnitConversionConsumer defines methods to set the contents of the
// UnitConversionViewController.
@protocol UnitConversionConsumer <NSObject>

// Tells the consumer to update the unit type title (volume, mass, etc) based on
// the user's new selected unit type.
- (void)updateUnitTypeTitle:(NSString*)unitTypeTitle;

// Tells the consumer to update the source unit based on the user's new selected
// source unit type.
- (void)updateSourceUnit:(NSUnit*)sourceUnit;

// Tells the consumer to update the target unit based on the user's new selected
// target unit type.
- (void)updateTargetUnit:(NSUnit*)targetUnit;

// Tells the consumer to update the source unit value.
- (void)updateSourceUnitValue:(double)sourceUnitValue;

// Tells the consumer to update the target unit value.
- (void)updateTargetUnitValue:(double)targetUnitValue;

// Tells the consumer to reload data from its
// UnitConversionTableViewController's data source. Must be called after using
// one of the update* methods.
- (void)reloadUnitTableView;

@end

#endif  // IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_CONSUMER_H_
