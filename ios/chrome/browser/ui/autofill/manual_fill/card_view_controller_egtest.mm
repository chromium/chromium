// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ManualFallbackAddPaymentMethodMatcher;
using chrome_test_util::ManualFallbackCreditCardIconMatcher;
using chrome_test_util::ManualFallbackCreditCardTableViewMatcher;
using chrome_test_util::ManualFallbackCreditCardTableViewWindowMatcher;
using chrome_test_util::ManualFallbackFormSuggestionViewMatcher;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackManagePaymentMethodsMatcher;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::SettingsCreditCardMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;

namespace {

const char kFormElementName[] = "CCName";
const char kFormElementCardNumber[] = "CCNo";
const char kFormElementCardExpirationMonth[] = "CCExpiresMonth";
const char kFormElementCardExpirationYear[] = "CCExpiresYear";

NSString* kLocalCardNumber = @"4111111111111111";
NSString* kLocalCardHolder = @"Test User";
// The local card's expiration month and year (only the last two digits) are set
// with next month and next year.
NSString* kLocalCardExpirationMonth =
    base::SysUTF8ToNSString(autofill::test::NextMonth());
NSString* kLocalCardExpirationYear =
    base::SysUTF8ToNSString(autofill::test::NextYear().substr(2, 2));

// Unicode characters used in card number:
//  - 0x0020 - Space.
//  - 0x2060 - WORD-JOINER (makes string undivisible).
constexpr char16_t separator[] = {0x2060, 0x0020, 0};
constexpr char16_t kMidlineEllipsis[] = {
    0x2022, 0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0x2022,
    0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0};
NSString* kObfuscatedNumberPrefix = base::SysUTF16ToNSString(
    kMidlineEllipsis + std::u16string(separator) + kMidlineEllipsis +
    std::u16string(separator) + kMidlineEllipsis + std::u16string(separator));

NSString* kLocalNumberObfuscated =
    [NSString stringWithFormat:@"%@1111", kObfuscatedNumberPrefix];

NSString* kServerNumberObfuscated =
    [NSString stringWithFormat:@"%@2109", kObfuscatedNumberPrefix];

const char kFormHTMLFile[] = "/credit_card.html";

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Waits for the keyboard to appear. Returns NO on timeout.
BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

// Opens the payment method manual fill view and verifies that the card view
// controller is visible afterwards.
void OpenPaymentMethodManualFillView() {
  id<GREYMatcher> button_to_tap;
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    button_to_tap = grey_accessibilityLabel(
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
  } else {
    button_to_tap = ManualFallbackCreditCardIconMatcher();
  }

  // Tap the button that'll open the payment method manual fill view.
  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];

  // Verify the card table view controller is visible.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Matcher for the expanded credit card manual fill view button.
id<GREYMatcher> CreditCardManualFillViewButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_CREDIT_CARD_AUTOFILL_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    nil);
}

// Matcher for the credit card tab in the manual fill view.
id<GREYMatcher> CreditCardManualFillViewTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_PAYMENT_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(
          grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID)),
      nil);
}

// Matcher for the overflow menu button shown in the payment method cells.
id<GREYMatcher> OverflowMenuButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_MANUAL_FALLBACK_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL),
      grey_interactable(), nullptr);
}

// Matcher for the "Edit" action made available by the overflow menu button.
id<GREYMatcher> OverflowMenuEditAction() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_IOS_EDIT_ACTION_TITLE),
                    grey_interactable(), nullptr);
}

// Matcher for the "Show Details" action made available by the overflow menu
// button.
id<GREYMatcher> OverflowMenuShowDetailsAction() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_SHOW_DETAILS_ACTION_TITLE),
      grey_interactable(), nullptr);
}

// Matcher for the "Autofill Form" button shown in the payment method cells.
id<GREYMatcher> AutofillFormButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_TITLE),
                    grey_interactable(), nullptr);
}

// Opens the payment method manual fill view when there are no saved payment
// methods and verifies that the card view controller is visible afterwards.
// Only useful when the `kIOSKeyboardAccessoryUpgrade` feature is enabled.
void OpenPaymentMethodManualFillViewWithNoSavedPaymentMethods() {
  // Tap the button to open the expanded manual fill view.
  [[EarlGrey selectElementWithMatcher:CreditCardManualFillViewButton()]
      performAction:grey_tap()];

  // Tap the payment method tab from the segmented control.
  [[EarlGrey selectElementWithMatcher:CreditCardManualFillViewTab()]
      performAction:grey_tap()];

  // Verify the card table view controller is visible.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace

// Integration Tests for Manual Fallback credit cards View Controller.
@interface CreditCardViewControllerTestCase : ChromeTestCase
@end

@implementation CreditCardViewControllerTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return YES;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableVirtualCards);

  if ([self shouldEnableKeyboardAccessoryUpgradeFeature]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  } else {
    config.features_disabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }

  return config;
}

#pragma mark - Tests

// Tests that the credit card view button is absent when there are no cards
// available.
- (void)testCreditCardsButtonAbsentWhenNoCreditCardsAvailable {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Verify there's no credit card icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card view controller appears on screen.
- (void)testCreditCardsViewControllerIsPresented {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view and verify that the card table
  // view controller is visible.
  OpenPaymentMethodManualFillView();
}

// Tests that the the "no payment methods found" message is visible when no
// payment method suggestions are available.
- (void)testNoPaymentMethodsFoundMessageIsVisibleWhenNoSuggestions {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is disabled.");
  }

  [AutofillAppInterface clearCreditCardStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillViewWithNoSavedPaymentMethods();

  // Assert that the "no payment methods found" message is visible.
  id<GREYMatcher> noPaymentMethodsFoundMessage = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_PAYMENT_METHODS));
  [[EarlGrey selectElementWithMatcher:noPaymentMethodsFoundMessage]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the cards view controller contains the "Manage Payment
// Methods..." action.
- (void)testCreditCardsViewControllerContainsManagePaymentMethodsAction {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Try to scroll.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the payment methods controller contains the "Manage Payment
  // Methods..." action.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackManagePaymentMethodsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the manual fallback view shows both a virtual card and the
// original card for a credit card with a virtual card status of enrolled.
- (void)testManualFallbackShowsVirtualCards {
  // Create & save credit card enrolled in virtual card program.
  [AutofillAppInterface saveMaskedCreditCardEnrolledInVirtualCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    // Scroll to the right to reach the credit card icon.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  }

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Assert presence of virtual card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"Mastercard \nVirtual card")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down to show original card.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 200)];

  // Assert presence of original card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mastercard ")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Clear server cards.
  [AutofillAppInterface clearAllServerDataForTesting];
}

// Tests that the manual fallback view for credit cards shows a label for each
// button.
- (void)testManualFallbackShowsCardLabeledButtons {
  // Create & save local credit card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    // Scroll to the right to reach the credit card icon.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  }

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Assert card number label.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Card number:")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert card number button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kLocalNumberObfuscated)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert expiration date label.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Expiration date:")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert expiration month button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kLocalCardExpirationMonth)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert expiration year button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kLocalCardExpirationYear)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert card holder name label.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name on card:")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert card holder name button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kLocalCardHolder)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Manage Payment Methods..." action works.
- (void)testManagePaymentMethodsActionOpensPaymentMethodSettings {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Try to scroll.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Manage Payment Methods..." action.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackManagePaymentMethodsMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsCreditCardMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view and icon is not highlighted after
// presenting the manage payment methods view.
- (void)testCreditCardsStateAfterPresentingPaymentMethodSettings {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Scroll to the right.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Try to scroll.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Manage Payment Methods..." action.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackManagePaymentMethodsMatcher()]
      performAction:grey_tap()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Verify the status of the icons.
    [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
        assertWithMatcher:grey_userInteractionEnabled()];
    [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];

    // Verify the keyboard is not cover by the cards view.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that the "Add Payment Method..." action works.
- (void)testAddPaymentMethodActionOpensAddPaymentMethodSettings {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  if (![ChromeEarlGrey isIPadIdiom]) {
    // Try to scroll on iPhone.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }

  // Tap the "Add Payment Method..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackAddPaymentMethodMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Add Payment Method..." action works on OTR.
- (void)testOTRAddPaymentMethodActionOpensAddPaymentMethodSettings {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Scroll if not iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }

  // Tap the "Add Payment Method..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackAddPaymentMethodMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view icon is not highlighted after presenting
// the add credit card view.
- (void)testCreditCardsButtonStateAfterPresentingAddCreditCard {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Scroll to the right.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Try to scroll.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Add Payment Method..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackAddPaymentMethodMatcher()]
      performAction:grey_tap()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Verify the status of the icons.
    [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:ManualFallbackCreditCardIconMatcher()]
        assertWithMatcher:grey_userInteractionEnabled()];
    [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];

    // Verify the keyboard is not cover by the cards view.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that the credit card View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissCreditCardController {
  if ([ChromeEarlGrey isIPadIdiom] ||
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The keyboard icon is never present on iPads or when the Keyboard "
        @"Accessory Upgrade feature is enabled.");
  }

  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// outside the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissCreditCardController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// keyboard.
// TODO(crbug.com/40250530): reenable this flaky test.
- (void)DISABLED_testTappingKeyboardDismissCreditCardControllerPopOver {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap a keyboard key directly. Typing with EG helpers do not trigger physical
  // keyboard presses.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"G")]
      performAction:grey_tap()];

  // As of Xcode 14 beta 2, tapping the keyboard does not dismiss the
  // accessory view popup.
  bool systemDismissesView = true;
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
  if (@available(iOS 16, *)) {
    systemDismissesView = false;
  }
#endif  // defined(__IPHONE_16_0)

  if (systemDismissesView) {
    // Verify the credit card controller table view and the credit card icon is
    // not visible.
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        assertWithMatcher:grey_notVisible()];
    [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
        assertWithMatcher:grey_notVisible()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Tests that, after switching fields, the content size of the table view didn't
// grow.
- (void)testCreditCardControllerKeepsRightSize {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the second element.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementCardNumber)];

  // Try to scroll.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
}

// Tests that the credit card View Controller stays on rotation.
- (void)testCreditCardControllerSupportsRotation {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  // Verify the credit card controller table view is still visible.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that credit card number (for local card) is injected.
// TODO(crbug.com/40577448): maybe figure a way to test successfull injection
// when page is https, but right now if we use the https embedded server,
// there's a warning page that stops the flow because of a
// NET::ERR_CERT_AUTHORITY_INVALID.
- (void)testCreditCardLocalNumberDoesntInjectOnHttp {
  [self verifyCreditCardButtonWithTitle:kLocalNumberObfuscated
                        doesInjectValue:@""];

  // Dismiss the warning alert.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI cleanupAfterShowingAlert];
}

// Tests an alert is shown warning the user when trying to fill a credit card
// number in an HTTP form.
- (void)testCreditCardLocalNumberShowsWarningOnHttp {
  [self verifyCreditCardButtonWithTitle:kLocalNumberObfuscated
                        doesInjectValue:@""];
  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];

  // Dismiss the alert.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI cleanupAfterShowingAlert];
}

// Tests that credit card cardholder is injected.
- (void)testCreditCardCardHolderInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardHolder
                        doesInjectValue:kLocalCardHolder];
}

// Tests that credit card expiration month is injected.
- (void)testCreditCardExpirationMonthInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardExpirationMonth
                        doesInjectValue:kLocalCardExpirationMonth];
}

// Tests that credit card expiration year is injected.
- (void)testCreditCardExpirationYearInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardExpirationYear
                        doesInjectValue:kLocalCardExpirationYear];
}

// Tests that masked credit card offer CVC input.
// TODO(crbug.com/41428751) can't test this one until https tests are possible.
- (void)DISABLED_testCreditCardServerNumberRequiresCVC {
  [AutofillAppInterface saveMaskedCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Wait for the accessory icon to appear.
  GREYAssert(WaitForKeyboardToAppear(), @"Keyboard didn't appear.");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Select a the masked number.
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(kServerNumberObfuscated)]
      performAction:grey_tap()];

  // Verify the CVC requester is visible.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Confirm Card")]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/40577448): maybe figure a way to enter CVC and get the
  // unlocked card result.
}

// Tests that the overflow menu button is only visible when the Keyboard
// Accessory Upgrade feature is enabled.
- (void)testOverflowMenuVisibility {
  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests the "Edit" action of the overflow menu button displays the card's
// details in edit mode.
- (void)testEditCardFromOverflowMenu {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // TODO(crbug.com/326413453): Check that the card details opened.
}

// Tests the "Show Details" action of the overflow menu button displays the
// card's details in edit mode.
- (void)testShowCardDetailsFromOverflowMenu {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the overflow menu button and select the "Show Details" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuShowDetailsAction()]
      performAction:grey_tap()];

  // TODO(crbug.com/326413453): Check that the card details opened.
}

// Tests that tapping the "Autofill Form" button fills the payment form with
// the right data.
- (void)testAutofillFormButtonFillsForm {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:autofill::test::GetCreditCard()];
}

#pragma mark - Private

- (void)verifyCreditCardButtonWithTitle:(NSString*)title
                        doesInjectValue:(NSString*)result {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Wait for the accessory icon to appear.
  GREYAssert(WaitForKeyboardToAppear(), @"Keyboard didn't appear.");

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Select a field.
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(title)]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementName, result];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

// Verify credit card infos are filled.
- (void)verifyCreditCardInfosHaveBeenFilled:(autofill::CreditCard)card {
  std::string locale = l10n_util::GetLocaleOverride();

  // Credit card name.
  NSString* name = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NAME_FULL, locale));
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementName, name];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card number.
  NSString* number = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NUMBER, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementCardNumber, number];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card expiration month.
  NSString* expMonth = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementCardExpirationMonth, expMonth];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card expiration year.
  NSString* expYear = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementCardExpirationYear, expYear];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

@end

// Rerun all the tests in this file but with kIOSKeyboardAccessoryUpgrade
// disabled. This will be removed once that feature launches fully, but ensures
// regressions aren't introduced in the meantime.
@interface CreditCardViewControllerKeyboardAccessoryUpgradeDisabledTestCase
    : CreditCardViewControllerTestCase

@end

@implementation CreditCardViewControllerKeyboardAccessoryUpgradeDisabledTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return NO;
}

// This causes the test case to actually be detected as a test case. The actual
// tests are all inherited from the parent class.
- (void)testEmpty {
}

@end
