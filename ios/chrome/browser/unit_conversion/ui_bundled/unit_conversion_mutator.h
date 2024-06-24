// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MUTATOR_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MUTATOR_H_
#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

@protocol UnitConversionMutator <NSObject>

// Notifies the mutator of a unit title change.
- (void)unitTypeDidChange:(ios::provider::UnitType)unitTypeTitle
                unitValue:(double)unitValue;

// Notifies the mutator of a source unit change.
- (void)sourceUnitDidChange:(NSUnit*)sourceUnit
                 targetUnit:(NSUnit*)targetUnit
                  unitValue:(double)unitValue
                   unitType:(ios::provider::UnitType)unitType;

// Notifies the mutator of a target unit change.
- (void)targetUnitDidChange:(NSUnit*)targetUnit
                 sourceUnit:(NSUnit*)sourceUnit
                  unitValue:(double)unitValue
                   unitType:(ios::provider::UnitType)unitType;

// Notifies the mutator of a source unit value change.
- (void)sourceUnitValueFieldDidChange:(NSString*)sourceUnitValueField
                           sourceUnit:(NSUnit*)sourceUnit
                           targetUnit:(NSUnit*)targetUnit;

// Notifies the mutator of a target unit value change.
- (void)targetUnitValueFieldDidChange:(NSString*)targetUnitValueField
                           sourceUnit:(NSUnit*)sourceUnit
                           targetUnit:(NSUnit*)targetUnit;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MUTATOR_H_
