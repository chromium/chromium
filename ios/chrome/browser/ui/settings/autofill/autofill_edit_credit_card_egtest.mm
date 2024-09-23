// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::TextFieldForCellWithLabelId;

// Tests for Settings Autofill edit credit cards screen.
@interface AutofillEditCreditCardTestCase : ChromeTestCase
@end

namespace {

// Matcher for the 'Nickname' text field in the edit credit card view.
id<GREYMatcher> NicknameTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_NICKNAME);
}

// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(chrome_test_util::TabGridEditButton()),
      grey_kindOfClass([UIButton class]),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

// Returns an action to scroll down (swipe up).
id<GREYAction> ScrollDown() {
  return grey_scrollInDirection(kGREYDirectionDown, 150);
}

// Matcher for the 'Year of Expiry' text field in the edit credit card view.
id<GREYMatcher> YearOfExpiryTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_EXP_YEAR);
}

}  // namespace

@implementation AutofillEditCreditCardTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Add feature configs here.
  return config;
}

- (void)setUp {
  [super setUp];

  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface setMandatoryReauthEnabled:YES];

  [AutofillAppInterface clearCreditCardStore];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(lastDigits)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearMockReauthenticationModule];
  [super tearDown];
}

#pragma mark - Test that all fields on the 'Add Credit Card' screen appear

// Tests that editing the credit card nickname is possible.
- (void)testValidNickname {
  [self typeNickname:@"Nickname"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that invalid nicknames are not allowed when editing a card.
- (void)testInvalidNickname {
  [self typeNickname:@"1233"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests that clearing a nickname is allowed.
- (void)testEmptyNickname {
  [self typeNickname:@"To be removed"];

  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
}

// Tests that the Done button in the navigation bar is disabled on entering
// invalid year of expiry in the edit credit card form.
- (void)testDoneOnInvalidYearInEditCreditCard {
  [[[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(@"2000")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

#pragma mark - Helper methods

// Scrolls to nickname text field and types the string.
- (void)typeNickname:(NSString*)nickname {
  [[[EarlGrey selectElementWithMatcher:NicknameTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(nickname)];
}

@end
