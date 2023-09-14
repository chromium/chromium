// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

namespace ios::provider {

NSUnit* GetDefaultUnitForType(UnitType unitType) {
  return nil;
}

std::vector<UnitType> GetSupportedUnitTypes() {
  return {};
}

NSUnit* GetDefaultTargetUnit(NSUnit* unit) {
  return nil;
}

const NSArray<NSArray<NSUnit*>*>* GetUnitsForType(UnitType unitType) {
  return nil;
}

NSString* GetFormattedUnit(NSUnit* unit) {
  return nil;
}

}  // namespace ios::provider
