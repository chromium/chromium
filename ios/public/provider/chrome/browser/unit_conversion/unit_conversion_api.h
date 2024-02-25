// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_API_H_

#import <UIKit/UIKit.h>

#include <vector>

namespace ios::provider {

// Unit Types
enum UnitType {
  kUnitTypeUnknown = 0,
  kUnitTypeArea = 1,
  kUnitTypeInformationStorage = 2,
  kUnitTypeLength = 3,
  kUnitTypeMass = 4,
  kUnitTypeSpeed = 5,
  kUnitTypeTemperature = 6,
  kUnitTypeVolume = 7
};

// Returns the default source unit based on the unit type.
NSUnit* GetDefaultUnitForType(UnitType unit_type);

// Returns the list of unit types.
std::vector<UnitType> GetSupportedUnitTypes();

// Returns the default target unit for a given unit.
NSUnit* GetDefaultTargetUnit(NSUnit* unit);

// Returns the list of the units given a unit type, the list can be divided into
// sections when the options metric/imperial are available.
const NSArray<NSArray<NSUnit*>*>* GetUnitsForType(UnitType unit_type);

// Returns a formatted string representing the given unit.
NSString* GetFormattedUnit(NSUnit* unit);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_API_H_
