// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_constants.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_consumer.h"
#import "ios/chrome/test/providers/unit_conversion/test_unit_conversion.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Source and target unit values for testing.
const double kSourceUnitValue = 1;
const double kExpectedSourceUnitValue = 1;
const double kExpectedTargetUnitValue = 1000;
const double kExpectedDefaultValue = 0;

// Source and target units value fields in valid format.
NSString* kValidSourceUnitValueField = @"1";
NSString* kValidTargetUnitValueField = @"1000";

// Source/Target unit value field in invalid format.
NSString* kInvalidUnitValueField = @"&1";

}  // namespace

// A Unit Conversion factory that return test source and target units.
@interface TestUnitConversionProviderTestHelper
    : NSObject <UnitConversionProviderTestHelper>
@end

@implementation TestUnitConversionProviderTestHelper

// Returns default units or nil to test nil units handling.
- (NSUnit*)sourceUnitFromUnitType:(ios::provider::UnitType)unit_type {
  if (unit_type == ios::provider::kUnitTypeMass) {
    return [NSUnitMass kilograms];
  }

  if (unit_type == ios::provider::kUnitTypeArea) {
    return [NSUnitArea squareMiles];
  }

  return nil;
}

// Returns default, invalid or nil units from a source unit.
- (NSUnit*)targetUnitFromUnit:(NSUnit*)unit {
  if ([unit isEqual:[NSUnitMass kilograms]]) {
    return [NSUnitMass grams];
  }

  if ([unit isEqual:[NSUnitArea squareMiles]]) {
    return [NSUnitVolume cubicMiles];
  }

  return nil;
}

@end

// Test the Unit Conversion mediator and its unit conversions.
class UnitConversionMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    mediator_ = [[UnitConversionMediator alloc] initWithService:&service_];
    helper_ = [[TestUnitConversionProviderTestHelper alloc] init];
    ios::provider::test::SetUnitConversionProviderTestHelper(helper_);
  }

  void TearDown() override {
    service_.Shutdown();
    ios::provider::test::SetUnitConversionProviderTestHelper(nil);
    PlatformTest::TearDown();
  }

 protected:
  UnitConversionMediator* mediator_;
  TestUnitConversionProviderTestHelper* helper_;
  UnitConversionService service_;
};

// Tests that the conversion and the updates are handled correctly when the
// source/traget unit conversion is possible (source and target are of the same
// type).
TEST_F(UnitConversionMediatorTest, TestPossibleUnitTypeChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;

  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateSourceUnit:[NSUnitMass kilograms] reload:NO]);
  OCMExpect([consumer_mock updateTargetUnit:[NSUnitMass grams] reload:NO]);
  OCMExpect([consumer_mock updateSourceUnitValue:kSourceUnitValue reload:NO]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:NO]);
  OCMExpect([consumer_mock updateUnitTypeTitle:ios::provider::kUnitTypeMass]);

  [mediator_ unitTypeDidChange:ios::provider::kUnitTypeMass
                     unitValue:kSourceUnitValue];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the metrics are recorded correctly after a unit type change
// followed with a source unit change.
TEST_F(UnitConversionMediatorTest, TestUnitTypeAndSourceUnitChanges) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateSourceUnit:source_unit reload:NO]);
  OCMExpect([consumer_mock updateTargetUnit:target_unit reload:NO]);
  OCMExpect([consumer_mock updateSourceUnitValue:kSourceUnitValue reload:NO]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:NO]);
  OCMExpect([consumer_mock updateUnitTypeTitle:ios::provider::kUnitTypeMass]);
  OCMExpect([consumer_mock updateSourceUnit:source_unit reload:YES]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:YES]);

  [mediator_ unitTypeDidChange:ios::provider::kUnitTypeMass
                     unitValue:kSourceUnitValue];
  [mediator_ sourceUnitDidChange:source_unit
                      targetUnit:target_unit
                       unitValue:kSourceUnitValue
                        unitType:ios::provider::kUnitTypeMass];

  [mediator_ reportMetrics];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);

  // Metric Test
  EXPECT_EQ(
      user_action_tester.GetActionCount("IOS.UnitConversion.UnitTypeChange"),
      1);
  histogram_tester.ExpectBucketCount(
      kSourceUnitChangeAfterUnitTypeChangeHistogram,
      UnitConversionActionTypes::kMassImperial, 1);
  histogram_tester.ExpectTotalCount(
      kSourceUnitChangeAfterUnitTypeChangeHistogram, 1);

  histogram_tester.ExpectBucketCount(
      kSourceUnitChangeBeforeUnitTypeChangeHistogram,
      UnitConversionActionTypes::kUnchanged, 1);
  histogram_tester.ExpectTotalCount(
      kSourceUnitChangeBeforeUnitTypeChangeHistogram, 1);
}

// Tests that no conversion and no consumer unit value update is taking place
// when the conversion is impossible (source and target unit are not of the same
// type).
TEST_F(UnitConversionMediatorTest, TestImpossibleUnitTypeChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;

  [consumer_mock setExpectationOrderMatters:YES];
  [mediator_ unitTypeDidChange:ios::provider::kUnitTypeArea
                     unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that no conversion and no consumer unit value update is taking place
// when the source/target are nil.
TEST_F(UnitConversionMediatorTest, TestNilUnitTypeChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;

  [consumer_mock setExpectationOrderMatters:YES];
  [mediator_ unitTypeDidChange:ios::provider::kUnitTypeUnknown
                     unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the source unit change is handled properly when the conversion is
// possible between source and target unit.
TEST_F(UnitConversionMediatorTest, TestPossibleSourceUnitChange) {
  base::HistogramTester histogram_tester;
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  [consumer_mock setExpectationOrderMatters:YES];
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  OCMExpect([consumer_mock updateSourceUnit:source_unit reload:YES]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:YES]);
  [mediator_ sourceUnitDidChange:source_unit
                      targetUnit:target_unit
                       unitValue:kSourceUnitValue
                        unitType:ios::provider::kUnitTypeMass];
  [mediator_ reportMetrics];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
  histogram_tester.ExpectBucketCount(
      kSourceUnitChangeBeforeUnitTypeChangeHistogram,
      UnitConversionActionTypes::kMassImperial, 1);
  histogram_tester.ExpectTotalCount(
      kSourceUnitChangeBeforeUnitTypeChangeHistogram, 1);

  histogram_tester.ExpectBucketCount(
      kSourceUnitChangeAfterUnitTypeChangeHistogram,
      UnitConversionActionTypes::kUnchanged, 1);
  histogram_tester.ExpectTotalCount(
      kSourceUnitChangeAfterUnitTypeChangeHistogram, 1);
}

// Tests that no update is taking place after a source unit change when the
// conversion is impossible (source/target units are of different types).
TEST_F(UnitConversionMediatorTest, TestImpossibleSourceUnitChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitArea* target_unit = [NSUnitArea squareMiles];
  [mediator_ sourceUnitDidChange:source_unit
                      targetUnit:target_unit
                       unitValue:kSourceUnitValue
                        unitType:ios::provider::kUnitTypeMass];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the target unit change is handled properly when the conversion is
// possible between source and target unit.
TEST_F(UnitConversionMediatorTest, TestPossibleTargetUnitChange) {
  base::HistogramTester histogram_tester;
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateTargetUnit:target_unit reload:YES]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:YES]);

  [mediator_ targetUnitDidChange:target_unit
                      sourceUnit:source_unit
                       unitValue:kSourceUnitValue
                        unitType:ios::provider::kUnitTypeMass];
  [mediator_ reportMetrics];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
  histogram_tester.ExpectBucketCount(
      kTargetUnitChangeHistogram, UnitConversionActionTypes::kMassImperial, 1);
  histogram_tester.ExpectTotalCount(kTargetUnitChangeHistogram, 1);
}

// Tests that no update is taking place after a target unit change when the
// conversion is impossible (source/target units are of different types).
TEST_F(UnitConversionMediatorTest, TestImpossibleTargetUnitChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitArea* target_unit = [NSUnitArea squareMiles];
  [mediator_ targetUnitDidChange:target_unit
                      sourceUnit:source_unit
                       unitValue:kSourceUnitValue
                        unitType:ios::provider::kUnitTypeMass];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the source field unit is handled properly when the string to
// NSNumber cast is possible.
TEST_F(UnitConversionMediatorTest, TestValidSourceUnitValueFieldChange) {
  base::UserActionTester user_action_tester;
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue
                                          reload:YES]);

  [mediator_ sourceUnitValueFieldDidChange:kValidSourceUnitValueField
                                sourceUnit:source_unit
                                targetUnit:target_unit];
  [mediator_ reportMetrics];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.UnitConversion.SourceUnitValueChange"),
            1);
}

// Tests that no update nor conversion is taking place when the source unit
// field value is invalid and that the target unit field is updated with the
// default value 0.
TEST_F(UnitConversionMediatorTest, TestInvalidSourceUnitValueFieldChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedDefaultValue
                                          reload:YES]);
  [mediator_ sourceUnitValueFieldDidChange:kInvalidUnitValueField
                                sourceUnit:source_unit
                                targetUnit:target_unit];
}

// Tests that the target field unit is handled properly when the string to
// NSNumber cast is possible.
TEST_F(UnitConversionMediatorTest, TestValidTargetUnitValueFieldChange) {
  base::UserActionTester user_action_tester;
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  OCMExpect([consumer_mock updateSourceUnitValue:kExpectedSourceUnitValue
                                          reload:YES]);

  [mediator_ targetUnitValueFieldDidChange:kValidTargetUnitValueField
                                sourceUnit:source_unit
                                targetUnit:target_unit];
  [mediator_ reportMetrics];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.UnitConversion.TargetUnitValueChange"),
            1);
}

// Tests that no update nor conversion is taking place when the target unit
// field value is invalid and that the source unit field is updated with the
// default value 0.
TEST_F(UnitConversionMediatorTest, TestInvalidTargetUnitValueFieldChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* source_unit = [NSUnitMass kilograms];
  NSUnitMass* target_unit = [NSUnitMass grams];
  OCMExpect([consumer_mock updateSourceUnitValue:kExpectedDefaultValue
                                          reload:YES]);
  [mediator_ targetUnitValueFieldDidChange:kInvalidUnitValueField
                                sourceUnit:source_unit
                                targetUnit:target_unit];
}
