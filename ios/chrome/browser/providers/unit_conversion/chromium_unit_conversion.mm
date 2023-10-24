// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

namespace ios::provider {

NSUnit* GetDefaultUnitForType(UnitType unitType) {
  if (unitType == kUnitTypeMass) {
    return [NSUnitMass kilograms];
  }
  if (unitType == kUnitTypeLength) {
    return [NSUnitLength miles];
  }
  return nil;
}

std::vector<UnitType> GetSupportedUnitTypes() {
  return {kUnitTypeMass, kUnitTypeLength};
}

NSUnit* GetDefaultTargetUnit(NSUnit* unit) {
  if ([unit isEqual:[NSUnitMass kilograms]]) {
    return [NSUnitMass poundsMass];
  }
  if ([unit isEqual:[NSUnitLength miles]]) {
    return [NSUnitLength yards];
  }
  return nil;
}

const NSArray<NSArray<NSUnit*>*>* GetUnitsForType(UnitType unitType) {
  if (unitType == kUnitTypeMass) {
    return @[ @[ [NSUnitMass kilograms] ], @[ [NSUnitMass poundsMass] ] ];
  }
  if (unitType == kUnitTypeLength) {
    return @[ @[ [NSUnitLength miles], [NSUnitLength yards] ] ];
  }
  return nil;
}

NSString* GetFormattedUnit(NSUnit* unit) {
  if ([unit isEqual:[NSUnitMass kilograms]]) {
    return @"Kilograms (kg)";
  }
  if ([unit isEqual:[NSUnitMass poundsMass]]) {
    return @"Pounds (lb)";
  }
  if ([unit isEqual:[NSUnitLength miles]]) {
    return @"Miles (mi)";
  }
  if ([unit isEqual:[NSUnitLength yards]]) {
    return @"Yards (yd)";
  }
  return nil;
}

}  // namespace ios::provider
