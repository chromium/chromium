// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_UNIT_CONVERSION_TEST_UNIT_CONVERSION_H_
#define IOS_CHROME_TEST_PROVIDERS_UNIT_CONVERSION_TEST_UNIT_CONVERSION_H_

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

// A protocol to replace the Unit Conversion providers in tests.
@protocol UnitConversionProviderTestHelper

- (NSUnit*)sourceUnitFromUnitType:(ios::provider::UnitType)unit_type;
- (NSUnit*)targetUnitFromUnit:(NSUnit*)unit;

@end

namespace ios::provider {
namespace test {

// Sets the global helper for the tests.
// Resets it if `helper` is nil.
void SetUnitConversionProviderTestHelper(
    id<UnitConversionProviderTestHelper> helper);

}  // namespace test
}  // namespace ios::provider

#endif  // IOS_CHROME_TEST_PROVIDERS_UNIT_CONVERSION_TEST_UNIT_CONVERSION_H_
