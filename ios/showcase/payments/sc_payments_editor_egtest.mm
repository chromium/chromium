// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UIWindow (Hidden)
- (UIResponder*)firstResponder;
@end

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;

// Returns the GREYMatcher for the input accessory view's previus button.
id<GREYMatcher> InputAccessoryViewPreviousButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the input accessory view's next button.
id<GREYMatcher> InputAccessoryViewNextButton() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_kindOfClass([UIButton class]), grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the input accessory view's close button.
id<GREYMatcher> InputAccessoryViewCloseButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

void AssertTextFieldWithAccessibilityIDIsFirstResponder(
    NSString* accessibilityID) {
  UIResponder* firstResponder =
      [[UIApplication sharedApplication].keyWindow firstResponder];
  GREYAssertTrue([firstResponder isKindOfClass:[UITextField class]],
                 @"Expected first responder to be of kind %@, got %@.",
                 [UITextField class], [firstResponder class]);
  UITextField* textField =
      base::mac::ObjCCastStrict<UITextField>(firstResponder);
  GREYAssertTrue(
      [[textField accessibilityIdentifier] isEqualToString:accessibilityID],
      @"Expected accessibility identifier to be %@, got %@.", accessibilityID,
      [textField accessibilityIdentifier]);
}

// Returns the GREYMatcher for the UIAlertView's message displayed for a call
// that notifies the delegate of selection of a field.
id<GREYMatcher> UIAlertViewMessageForDelegateCallWithArgument(
    NSString* argument) {
  return grey_allOf(
      grey_text([NSString
          stringWithFormat:@"paymentRequestEditViewController:"
                           @"kPaymentRequestEditCollectionViewAccessibilityID "
                           @"didSelectField:%@",
                           argument]),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the next key on the keyboard.
id<GREYMatcher> KeyboardNextKey() {
  return grey_allOf(grey_anyOf(grey_accessibilityID(@"Next"),
                               grey_accessibilityID(@"Next:"), nil),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_accessibilityTrait(UIAccessibilityTraitKeyboardKey),
                    grey_sufficientlyVisible(), nil);
}

}  // namespace

// Tests for the payment request editor view controller.
@interface SCPaymentsEditorTestCase : ShowcaseTestCase
@end

@implementation SCPaymentsEditorTestCase

- (void)setUp {
  [super setUp];
  Open(@"PaymentRequestEditViewController");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests if expected labels and fields exist and have the expected values.
- (void)testVerifyLabelsAndFields {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Name*")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      assertWithMatcher:grey_text(@"John Doe")];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Country*"),
                                          grey_accessibilityValue(@"Canada"),
                                          nil)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"City/Province*")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"City/Province_textField")]
      assertWithMatcher:grey_text(@"Montreal / Quebec")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Address*")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Address_textField")]
      assertWithMatcher:grey_text(@"")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Postal Code")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Postal Code_textField")]
      assertWithMatcher:grey_text(@"")];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Save"),
                                          grey_accessibilityValue(
                                              l10n_util::GetNSString(
                                                  IDS_IOS_SETTING_ON)),
                                          nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests if the expected input view for the province field is displaying, when
// the field is focused, and that the expected row is selected.
- (void)testVerifyProvinceFieldInputView {
  // Tap the province textfield.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"City/Province_textField")]
      performAction:grey_tap()];

  // Assert that a UIPicker view is displaying and the expected rows are
  // selected.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"City/Province_pickerView")]
      assertWithMatcher:grey_allOf(grey_pickerColumnSetToValue(0, @"Montreal"),
                                   grey_pickerColumnSetToValue(1, @"Quebec"),
                                   nil)];
}

// Tests if tapping the selector field notifies the delegate.
- (void)testVerifyTappingSelectorFieldNotifiesDelegate {
  // Tap the selector field.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Country*"),
                                          grey_accessibilityValue(@"Canada"),
                                          nil)] performAction:grey_tap()];

  // Confirm the delegate is informed.
  [[EarlGrey
      selectElementWithMatcher:UIAlertViewMessageForDelegateCallWithArgument(
                                   @"Label: Country, Value: CAN")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];
}

// Tests whether tapping the input accessory view's close button dismisses the
// input accessory view.
- (void)testInputAccessoryViewCloseButton {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/602666): Investigate why the close button is hidden on
    // iPad.
    EARL_GREY_TEST_DISABLED(
        @"Input accessory view's close button is hidden on iPad");
  }

  // Initially, the input ​accessory view is not showing.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormInputAccessoryViewAccessibilityID)]
      assertWithMatcher:grey_nil()];

  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the input accessory view's close button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewCloseButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Tapping the input accessory view's close button should've dismissed it.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormInputAccessoryViewAccessibilityID)]
      assertWithMatcher:grey_nil()];
}

// Tests whether the input accessory view navigation buttons have the correct
// states depending on the focused textfield and that they can be used to
// navigate between the textfields.
- (void)testInputAccessoryViewNavigationButtons {
  // Initially, no error message is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WarningMessageView()]
      assertWithMatcher:grey_nil()];

  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the name textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Name_textField");

  // Assert the input accessory view's previous button is disabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  // Assert the input accessory view's next button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the province textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(
      @"City/Province_textField");

  // Assert the input accessory view's previous button is enabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()];
  // Assert the input accessory view's next button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the address textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Address_textField");

  // Assert the input accessory view's previous button is enabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()];
  // Assert the input accessory view's next button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert an error message is showing because the address textfield is
  // required.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WarningMessageView()]
      assertWithMatcher:grey_accessibilityLabel(@"Field is required")];

  // Assert the postal code textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Postal Code_textField");

  // Assert the input accessory view's next button is disabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  // Assert the input accessory view's previous button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the address textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Address_textField");

  // Type in an address.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Address_textField")]
      performAction:grey_replaceText(@"Main St")];

  // Tap the input accessory view's next button.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      performAction:grey_tap()];

  // Assert the error message disappeared because an address was typed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WarningMessageView()]
      assertWithMatcher:grey_notVisible()];
}

// Tests tapping the return key on every textfield causes the next textfield to
// get focus except for the last textfield in which case causes the focus to go
// away from the textfield.
// TODO(crbug.com/997938): Test is Flaky on iOS13 iPad.
- (void)FLAKY_testNavigationByTappingReturn {
  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the name textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Name_textField");

  // Press the return key.
  [[EarlGrey selectElementWithMatcher:KeyboardNextKey()]
      performAction:grey_tap()];

  // Assert the province textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(
      @"City/Province_textField");

  // The standard keyboard does not display for the province field. Instead, tap
  // the postal code textfield.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(@"Postal Code_textField"),
                 grey_interactable(), grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:
          grey_accessibilityID(
              @"kPaymentRequestEditCollectionViewAccessibilityID")]
      performAction:grey_tap()];

  // Assert the postal code textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Postal Code_textField");

  // Press the return key.
  [[EarlGrey selectElementWithMatcher:KeyboardNextKey()]
      performAction:grey_tap()];

  // Expect non of the textfields to be focused.
  UIResponder* firstResponder =
      [[UIApplication sharedApplication].keyWindow firstResponder];
  GREYAssertFalse([firstResponder isKindOfClass:[UITextField class]],
                  @"Expected first responder not to be of kind %@.",
                  [UITextField class]);
}

@end
