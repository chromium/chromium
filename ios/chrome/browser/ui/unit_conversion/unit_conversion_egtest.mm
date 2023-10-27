// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_app_interface.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

// Matcher for the unit conversion view controller.
id<GREYMatcher> UnitConversionMatcher() {
  return grey_accessibilityID(kUnitConversionTableViewIdentifier);
}

// Matcher for the source unit label for a given unit label.
id<GREYMatcher> SourceUnitLabelMatcher(NSString* unit_label) {
  return grey_allOf(grey_accessibilityID(kSourceUnitLabelIdentifier),
                    testing::ElementWithAccessibilityLabelSubstring(unit_label),
                    nil);
}

// Matcher for the source unit field.
id<GREYMatcher> SourceUnitFieldMatcher() {
  return grey_accessibilityID(kSourceUnitFieldIdentifier);
}

// Matcher for the target unit label for a given unit label.
id<GREYMatcher> TargetUnitLabelMatcher(NSString* unit_label) {
  return grey_allOf(grey_accessibilityID(kTargetUnitLabelIdentifier),
                    testing::ElementWithAccessibilityLabelSubstring(unit_label),
                    nil);
}

// Matcher for the target unit field.
id<GREYMatcher> TargetUnitFieldMatcher() {
  return grey_accessibilityID(kTargetUnitFieldIdentifier);
}

// Taps on `item_button`.
void TapOnButton(id<GREYMatcher> item_button) {
  [[EarlGrey selectElementWithMatcher:item_button]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:item_button] performAction:grey_tap()];
}

// Matcher for the unit conversion view controller title button for the mass
// unit type.
id<GREYMatcher> MassButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_UNITS_MEASUREMENTS_MASS),
                    grey_interactable(), nil);
}

// Matcher for the unit conversion view controller title button for the length
// unit type.
id<GREYMatcher> LengthButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_UNITS_MEASUREMENTS_LENGTH),
                    grey_interactable(), nil);
}

// Matcher for the source unit menu button.
id<GREYMatcher> SourceUnitMenuButton() {
  return grey_accessibilityID(kSourceUnitMenuButtonIdentifier);
}

// Matcher for the target unit menu button.
id<GREYMatcher> TargetUnitMenuButton() {
  return grey_accessibilityID(kTargetUnitMenuButtonIdentifier);
}

// Matcher for the unit button in a displayed context menu.
id<GREYMatcher> UnitButtonWithLabel(NSString* label) {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(label),
                    grey_interactable(), nil);
}

}  // namespace

@interface UnitConversionTestCase : ChromeTestCase
@end

@implementation UnitConversionTestCase

- (void)tearDown {
  [UnitConversionAppInterface stopPresentingUnitConversionFeature];
  [super tearDown];
}

#pragma mark - Tests

// Test the elements of the unit conversion view controller
- (void)testUnitConversionViewController {
  [UnitConversionAppInterface presentUnitConversionFeature];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UnitConversionMatcher()];

  [[EarlGrey selectElementWithMatcher:SourceUnitLabelMatcher(@"Kilograms (kg)")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TargetUnitLabelMatcher(@"Pounds (lb)")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SourceUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"20.000000")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TargetUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"44.092488")];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_UNITS_MEASUREMENTS_MASS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_UNITS_MEASUREMENTS_REPORT_AN_ISSUE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that unit type change is handled correctly.
- (void)testUnitConversionUnitTypeChange {
  [UnitConversionAppInterface presentUnitConversionFeature];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UnitConversionMatcher()];

  TapOnButton(MassButton());

  TapOnButton(LengthButton());

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:MassButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_UNITS_MEASUREMENTS_LENGTH)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SourceUnitLabelMatcher(@"Miles (mi)")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SourceUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"20.000000")];

  [[EarlGrey selectElementWithMatcher:TargetUnitLabelMatcher(@"Yards (yd)")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TargetUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue([NSString
                            localizedStringWithFormat:@"%lf", 35200.0])];
}

// Checks that source unit value change is handled correctly.
- (void)testSourceUnitValueChange {
  [UnitConversionAppInterface presentUnitConversionFeature];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UnitConversionMatcher()];
  [[EarlGrey selectElementWithMatcher:SourceUnitFieldMatcher()]
      performAction:grey_replaceText(@"30")];
  [[EarlGrey selectElementWithMatcher:SourceUnitFieldMatcher()]
      assertWithMatcher:grey_textFieldValue(@"30")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TargetUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"66.138733")];
}

// Checks that the source unit change is handled correctly.
- (void)testSourceUnitChange {
  [UnitConversionAppInterface presentUnitConversionFeature];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UnitConversionMatcher()];
  TapOnButton(SourceUnitMenuButton());
  TapOnButton(UnitButtonWithLabel(@"Pounds (lb)"));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TargetUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"20.000000")];
}

// Checks that the target unit change is handled correctly.
- (void)testTargetUnitChange {
  [UnitConversionAppInterface presentUnitConversionFeature];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UnitConversionMatcher()];
  TapOnButton(TargetUnitMenuButton());
  TapOnButton(UnitButtonWithLabel(@"Kilograms (kg)"));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TargetUnitFieldMatcher(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_textFieldValue(@"20.000000")];
}

@end
