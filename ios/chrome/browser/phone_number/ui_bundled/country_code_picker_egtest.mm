// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_app_interface.h"
#import "ios/chrome/browser/phone_number/ui_bundled/phone_number_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "testing/gtest/include/gtest/gtest.h"

@interface CountryCodePickerTestCase : ChromeTestCase
@end

@implementation CountryCodePickerTestCase

- (void)tearDown {
  [CountryCodePickerAppInterface stopPresentingCountryCodePicker];
  [super tearDown];
}

// Tests the adding of a country code to a given phoner number and that the
// appropiate actions are displayed once the `Add` button is pressed.
- (void)testAddingCountryCodeToPhoneNumberAndAppropriateActions {
  [CountryCodePickerAppInterface presentCountryCodePicker];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kCountryCodePickerTableViewIdentifier)];

  // Tap on the first country code to add as a prefix.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   @"Afghanistan")] performAction:grey_tap()];

  // Tap on `Add` button.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_PHONE_NUMBER_ADD),
                            grey_not(grey_accessibilityTrait(
                                UIAccessibilityTraitNotEnabled)),
                            nil)] performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(kPhoneNumberActionsViewIdentifier)];

  // Check the different buttons.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PHONE_NUMBER_CALL)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PHONE_NUMBER_SEND_MESSAGE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PHONE_NUMBER_ADD_TO_CONTACTS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PHONE_NUMBER_FACETIME)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
