// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/unit_conversion/test_unit_conversion.h"

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

namespace {
id<UnitConversionProviderTestHelper> g_unit_conversion_provider_test_helper;
}

namespace ios::provider {

NSUnit* GetDefaultUnitForType(UnitType unitType) {
  return
      [g_unit_conversion_provider_test_helper sourceUnitFromUnitType:unitType];
}

std::vector<UnitType> GetSupportedUnitTypes() {
  return {};
}

NSUnit* GetDefaultTargetUnit(NSUnit* unit) {
  return [g_unit_conversion_provider_test_helper targetUnitFromUnit:unit];
}

const NSArray<NSArray<NSUnit*>*>* GetUnitsForType(UnitType unitType) {
  return nil;
}

NSString* GetFormattedUnit(NSUnit* unit) {
  return nil;
}

namespace test {

void SetUnitConversionProviderTestHelper(
    id<UnitConversionProviderTestHelper> helper) {
  g_unit_conversion_provider_test_helper = helper;
}

}  // namespace test

}  // namespace ios::provider
