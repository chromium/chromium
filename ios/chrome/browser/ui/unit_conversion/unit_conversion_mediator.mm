// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mediator.h"

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_consumer.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

@implementation UnitConversionMediator

#pragma mark - UnitConversionMutator

- (void)unitTypeDidChange:(ios::provider::UnitType)unitType
                unitValue:(double)unitValue {
  NSUnit* sourceUnit = ios::provider::GetDefaultUnitForType(unitType);
  NSUnit* targetUnit = ios::provider::GetDefaultTargetUnit(sourceUnit);

  if (sourceUnit && targetUnit) {
    NSMeasurement* sourceUnitMeasurement =
        [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
    if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
      NSMeasurement* targetUnitMeasurement =
          [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];
      [self.consumer updateSourceUnit:sourceUnit];
      [self.consumer updateTargetUnit:targetUnit];
      [self.consumer updateSourceUnitValue:sourceUnitMeasurement.doubleValue];
      [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue];
      [self.consumer reloadUnitTableView];
    }
  }
}

- (void)sourceUnitDidChange:(NSUnit*)sourceUnit
                 targetUnit:(NSUnit*)targetUnit
                  unitValue:(double)unitValue {
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];

    [self.consumer updateSourceUnit:sourceUnit];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue];
    [self.consumer reloadUnitTableView];
  }
}

- (void)targetUnitDidChange:(NSUnit*)targetUnit
                 sourceUnit:(NSUnit*)sourceUnit
                  unitValue:(double)unitValue {
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];

    [self.consumer updateTargetUnit:targetUnit];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue];
    [self.consumer reloadUnitTableView];
  }
}

- (void)sourceUnitValueFieldDidChange:(NSString*)sourceUnitValueField
                           sourceUnit:(NSUnit*)sourceUnit
                           targetUnit:(NSUnit*)targetUnit {
  NSNumber* unitValueNumber = [self numberFromString:sourceUnitValueField];
  if (!unitValueNumber) {
    return;
  }
  double unitValue = unitValueNumber.doubleValue;
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue];
    [self.consumer reloadUnitTableView];
  }
}

#pragma mark - Private

// Convert a string to a NSNumber*, returns nil if not valid.
- (NSNumber*)numberFromString:(NSString*)string {
  NSNumberFormatter* numberFormatter = [[NSNumberFormatter alloc] init];
  numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;
  return [numberFormatter numberFromString:string];
}

@end
