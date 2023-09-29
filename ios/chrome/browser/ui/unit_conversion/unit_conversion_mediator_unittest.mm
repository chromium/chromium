// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mediator.h"

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_consumer.h"
#import "ios/chrome/test/providers/unit_conversion/test_unit_conversion.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Source and target unit values for testing.
const double kSourceUnitValue = 1;
const double kExpectedTargetUnitValue = 1000;

// Source unit value field in valid format.
NSString* kValidSourceUnitValueField = @"1";

// Source unit value field in invalid format.
NSString* kInvalidSourceUnitValueField = @"&1";

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
    mediator_ = [[UnitConversionMediator alloc] init];
    helper_ = [[TestUnitConversionProviderTestHelper alloc] init];
    ios::provider::test::SetUnitConversionProviderTestHelper(helper_);
  }

  void TearDown() override {
    ios::provider::test::SetUnitConversionProviderTestHelper(nil);
    PlatformTest::TearDown();
  }

 protected:
  UnitConversionMediator* mediator_;
  TestUnitConversionProviderTestHelper* helper_;
};

// Tests that the conversion and the updates are handled correctly when the
// source/traget unit conversion is possible (source and target are of the same
// type).
TEST_F(UnitConversionMediatorTest, TestPossibleUnitTypeChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;

  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateSourceUnit:[OCMArg any]]);
  OCMExpect([consumer_mock updateTargetUnit:[OCMArg any]]);
  OCMExpect([consumer_mock updateSourceUnitValue:kSourceUnitValue]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue]);
  OCMExpect([consumer_mock reloadUnitTableView]);

  [mediator_ unitTypeDidChange:ios::provider::kUnitTypeMass
                     unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
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
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  [consumer_mock setExpectationOrderMatters:YES];
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitMass* targetUnit = [NSUnitMass grams];
  OCMExpect([consumer_mock updateSourceUnit:sourceUnit]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue]);
  OCMExpect([consumer_mock reloadUnitTableView]);
  [mediator_ sourceUnitDidChange:sourceUnit
                      targetUnit:targetUnit
                       unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that no update is taking place after a source unit change when the
// conversion is impossible (source/target units are of different types).
TEST_F(UnitConversionMediatorTest, TestImpossibleSourceUnitChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitArea* targetUnit = [NSUnitArea squareMiles];
  [mediator_ sourceUnitDidChange:sourceUnit
                      targetUnit:targetUnit
                       unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the target unit change is handled properly when the conversion is
// possible between source and target unit.
TEST_F(UnitConversionMediatorTest, TestPossibleTargetUnitChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitMass* targetUnit = [NSUnitMass grams];
  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateTargetUnit:targetUnit]);
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue]);
  OCMExpect([consumer_mock reloadUnitTableView]);

  [mediator_ targetUnitDidChange:targetUnit
                      sourceUnit:sourceUnit
                       unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that no update is taking place after a target unit change when the
// conversion is impossible (source/target units are of different types).
TEST_F(UnitConversionMediatorTest, TestImpossibleTargetUnitChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitArea* targetUnit = [NSUnitArea squareMiles];
  [mediator_ targetUnitDidChange:targetUnit
                      sourceUnit:sourceUnit
                       unitValue:kSourceUnitValue];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that the source field unit is handled properly when the string to
// NSNumber cast is possible.
TEST_F(UnitConversionMediatorTest, TestValidSourceUnitValueFieldChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitMass* targetUnit = [NSUnitMass grams];
  [consumer_mock setExpectationOrderMatters:YES];
  OCMExpect([consumer_mock updateTargetUnitValue:kExpectedTargetUnitValue]);
  OCMExpect([consumer_mock reloadUnitTableView]);

  [mediator_ sourceUnitValueFieldDidChange:kValidSourceUnitValueField
                                sourceUnit:sourceUnit
                                targetUnit:targetUnit];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}

// Tests that no update nor conversion is taking place when the source unit
// field value is invalid.
TEST_F(UnitConversionMediatorTest, TestInvalidSourceUnitValueFieldChange) {
  id consumer_mock = OCMStrictProtocolMock(@protocol(UnitConversionConsumer));
  mediator_.consumer = consumer_mock;
  NSUnitMass* sourceUnit = [NSUnitMass kilograms];
  NSUnitMass* targetUnit = [NSUnitMass grams];
  [mediator_ sourceUnitValueFieldDidChange:kInvalidSourceUnitValueField
                                sourceUnit:sourceUnit
                                targetUnit:targetUnit];
  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_mock);
}
