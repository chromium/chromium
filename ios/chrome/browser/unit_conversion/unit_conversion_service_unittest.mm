// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/unit_conversion_service.h"
#import "ios/chrome/test/providers/unit_conversion/test_unit_conversion.h"
#import "testing/platform_test.h"

// A Unit Conversion factory that return test source and target units.
@interface TestUnitConversionProviderTestHelperService
    : NSObject <UnitConversionProviderTestHelper>
@end

@implementation TestUnitConversionProviderTestHelperService

- (NSUnit*)sourceUnitFromUnitType:(ios::provider::UnitType)unit_type {
  return nil;
}

- (NSUnit*)targetUnitFromUnit:(NSUnit*)unit {
  return [NSUnitMass grams];
}

@end

// Test the Unit Conversion Service.
class UnitConversionServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    helper_ = [[TestUnitConversionProviderTestHelperService alloc] init];
    ios::provider::test::SetUnitConversionProviderTestHelper(helper_);
  }

  void TearDown() override {
    service_.Shutdown();
    ios::provider::test::SetUnitConversionProviderTestHelper(nil);
    PlatformTest::TearDown();
  }

 protected:
  UnitConversionService service_;
  TestUnitConversionProviderTestHelperService* helper_;
};

// Tests that the default conversion is taking place (the provided conversion
// from `ios_internal`) when no update has been made to
// `default_conversion_cache_`.
TEST_F(UnitConversionServiceTest, TestDefaultConversionFromProvider) {
  EXPECT_EQ(service_.GetDefaultTargetFromUnit([NSUnitMass grams]),
            [NSUnitMass grams]);
  service_.UpdateDefaultConversionCache([NSUnitMass grams],
                                        [NSUnitMass kilograms]);
  EXPECT_EQ(service_.GetDefaultTargetFromUnit([NSUnitMass grams]),
            [NSUnitMass kilograms]);
}

// Tests that the default conversion is changed after adding an element to
// `default_conversion_cache_`.
TEST_F(UnitConversionServiceTest, TestUpdatedDefaultConversion) {
  EXPECT_EQ(service_.GetDefaultTargetFromUnit([NSUnitMass grams]),
            [NSUnitMass grams]);
  service_.UpdateDefaultConversionCache([NSUnitMass grams],
                                        [NSUnitMass kilograms]);
  EXPECT_EQ(service_.GetDefaultTargetFromUnit([NSUnitMass grams]),
            [NSUnitMass kilograms]);
}
