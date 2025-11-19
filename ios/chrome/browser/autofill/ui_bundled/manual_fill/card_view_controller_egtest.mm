// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
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

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsCreditCardMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;

namespace {

const char kFormElementName[] = "CCName";
const char kFormElementCardNumber[] = "CCNo";
const char kFormElementCardExpirationMonth[] = "CCExpiresMonth";
const char kFormElementCardExpirationYear[] = "CCExpiresYear";

NSString* const kLocalCardNumber = @"4111111111111111";
NSString* const kLocalCardHolder = @"Test User";
// The local card's expiration month and year (only the last two digits) are set
// with next month and next year.
NSString* const kLocalCardExpirationMonth =
    base::SysUTF8ToNSString(autofill::test::NextMonth());
NSString* const kLocalCardExpirationYear =
    base::SysUTF8ToNSString(autofill::test::NextYear().substr(2, 2));

// Unicode characters used in card number:
//  - 0x0020 - Space.
//  - 0x2060 - WORD-JOINER (makes string undivisible).
constexpr char16_t separator[] = {0x2060, 0x0020, 0};
constexpr char16_t kMidlineEllipsis[] = {
    0x2022, 0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0x2022,
    0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0};
NSString* const kObfuscatedNumberPrefix = base::SysUTF16ToNSString(
    kMidlineEllipsis + std::u16string(separator) + kMidlineEllipsis +
    std::u16string(separator) + kMidlineEllipsis + std::u16string(separator));

NSString* const kLocalNumberObfuscated =
    [NSString stringWithFormat:@"%@1111", kObfuscatedNumberPrefix];

NSString* const kServerNumberObfuscated =
    [NSString stringWithFormat:@"%@2109", kObfuscatedNumberPrefix];
NSString* const kCvcObfuscated =
    base::SysUTF16ToNSString(autofill::CreditCard::GetMidlineEllipsisDots(3));

const char kFormHTMLFile[] = "/credit_card.html";

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Opens the payment method manual fill view and verifies that the card view
// controller is visible afterwards.
void OpenPaymentMethodManualFillView() {
  // Tap the button that'll open the payment method manual fill view.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::KeyboardAccessoryManualFillButton()]
      performAction:grey_tap()];

  // Verify the card table view controller is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
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
id<GREYMatcher> OverflowMenuButton(NSInteger cell_index) {
  return grey_allOf(grey_accessibilityID([ManualFillUtil
                        expandedManualFillOverflowMenuID:cell_index]),
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
  return grey_allOf(grey_accessibilityID(
                        manual_fill::kExpandedManualFillAutofillFormButtonID),
                    grey_interactable(), nullptr);
}

// Matcher for the GPay icon shown in the server card cells.
id<GREYMatcher> GPayIcon(NSString* network_and_last_four_digits) {
  return grey_accessibilityID([NSString
      stringWithFormat:@"%@ %@", manual_fill::kPaymentManualFillGPayLogoID,
                       network_and_last_four_digits]);
}

// Matcher for the card number chip button.
id<GREYMatcher> LocalCardNumberChipButton() {
  NSString* last_four_digits =
      [kLocalCardNumber substringFromIndex:kLocalCardNumber.length - 4];
  NSString* last_four_digits_split = [NSString
      stringWithFormat:@"%C %C %C %C", [last_four_digits characterAtIndex:0],
                       [last_four_digits characterAtIndex:1],
                       [last_four_digits characterAtIndex:2],
                       [last_four_digits characterAtIndex:3]];
  NSString* accessibility_label = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_CARD_NUMBER_CHIP_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(last_four_digits_split));

  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Matcher for the expiration month chip button.
id<GREYMatcher> ExpirationMonthChipButton(std::u16string title) {
  NSString* accessibility_label = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_EXPIRATION_MONTH_CHIP_ACCESSIBILITY_LABEL, title);
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Matcher for the expiration month chip button.
id<GREYMatcher> ExpirationYearChipButton(std::u16string title) {
  NSString* accessibility_label = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_EXPIRATION_YEAR_CHIP_ACCESSIBILITY_LABEL, title);
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Matcher for the cardholder month chip button.
id<GREYMatcher> CardholderChipButton(std::u16string title) {
  NSString* accessibility_label = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_CARDHOLDER_CHIP_ACCESSIBILITY_LABEL, title);
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Matcher for the cvc chip button.
id<GREYMatcher> CvcChipButton() {
  NSString* accessibility_label = l10n_util::GetNSString(
      IDS_IOS_MANUAL_FALLBACK_CVC_CHIP_ACCESSIBILITY_LABEL);
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Verifies that the number of accepted suggestions recorded for the given
// `suggestion_index` is as expected.
void CheckAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index) {
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:
                             @"Autofill.SuggestionAcceptedIndex.CreditCard"],
      @"Unexpected histogram count for accepted card suggestion index.");

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:@"Autofill.UserAcceptedSuggestionAtIndex."
                                      @"CreditCard.ManualFallback"],
      @"Unexpected histogram count for manual fallback accepted card "
      @"suggestion index.");
}

// Checks that the chip buttons of the local card are all visible.
void CheckChipButtonsOfLocalCard() {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  std::string locale = l10n_util::GetLocaleOverride();

  if (base::ios::IsRunningOnIOS18OrLater()) {
  } else {
    // On iOS 17.5, a rendering issue in tests prevents some cells from
    // displaying correctly. This scroll action ensures their proper visibility.
    [[EarlGrey
        selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
        performAction:grey_scrollInDirection(kGREYDirectionDown, 10)];
  }

  [[EarlGrey selectElementWithMatcher:LocalCardNumberChipButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ExpirationMonthChipButton(card.GetInfo(
                                   autofill::CREDIT_CARD_EXP_MONTH, locale))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ExpirationYearChipButton(card.GetInfo(
                                   autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                   locale))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:CardholderChipButton(card.GetInfo(
                                   autofill::CREDIT_CARD_NAME_FULL, locale))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Opens the payment method manual fill view when there are no saved payment
// methods and verifies that the card view controller is visible afterwards.
void OpenPaymentMethodManualFillViewWithNoSavedPaymentMethods() {
  // Tap the button to open the expanded manual fill view.
  [[EarlGrey selectElementWithMatcher:CreditCardManualFillViewButton()]
      performAction:grey_tap()];

  // Tap the payment method tab from the segmented control.
  [[EarlGrey selectElementWithMatcher:CreditCardManualFillViewTab()]
      performAction:grey_tap()];

  // Verify the card table view controller is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Dismisses the payment bottom sheet by tapping the "Use Keyboard" button.
void DismissPaymentBottomSheet() {
  id<GREYMatcher> useKeyboardButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_PAYMENT_BOTTOM_SHEET_USE_KEYBOARD);
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];
}

}  // namespace

// Integration Tests for Manual Fallback credit cards View Controller.
@interface CreditCardViewControllerTestCase : ChromeTestCase
@end

@implementation CreditCardViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [self loadURL];
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearAllServerDataForTesting];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  // Set up histogram tester.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearAllServerDataForTesting];
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];

  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCvcStorageAndFilling);
  return config;
}

#pragma mark - Tests

// Tests that the credit card view controller appears on screen.
- (void)testCreditCardsViewControllerIsPresented {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view and verify that the card table
  // view controller is visible.
  OpenPaymentMethodManualFillView();

  // Verify that the number of visible suggestions in the keyboard accessory was
  // correctly recorded.
  NSString* histogram =
      @"ManualFallback.VisibleSuggestions.ExpandIcon.OpenPaymentMethods";
  GREYAssertNil(
      [MetricsAppInterface expectUniqueSampleWithCount:1
                                             forBucket:1
                                          forHistogram:histogram],
      @"Unexpected histogram error for number of visible suggestions.");
}

// TODO(crbug.com/460721951): Test is failing on ios-simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testCardChipButtonsAreAllVisible \
  DISABLED_testCardChipButtonsAreAllVisible
#else
#define MAYBE_testCardChipButtonsAreAllVisible testCardChipButtonsAreAllVisible
#endif

// Tests that the saved card chip buttons are all visible in the card
// table view controller, and that they have the right accessibility label.
- (void)MAYBE_testCardChipButtonsAreAllVisible {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  CheckChipButtonsOfLocalCard();
}

// Tests that the the "no payment methods found" message is visible when no
// payment method suggestions are available.
- (void)testNoPaymentMethodsFoundMessageIsVisibleWhenNoSuggestions {
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

  // Verify that the number of visible suggestions in the keyboard accessory was
  // correctly recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:0
                         forHistogram:@"ManualFallback.VisibleSuggestions."
                                      @"OpenCreditCards"],
      @"Unexpected histogram error for number of visible suggestions.");
}

// Tests that the cards view controller contains the "Manage Payment
// Methods..." action.
- (void)testCreditCardsViewControllerContainsManagePaymentMethodsAction {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the payment methods controller contains the "Manage Payment
  // Methods..." action.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ManagePaymentMethodsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the manual fallback view shows both a virtual card and the
// original card for a credit card with a virtual card status of enrolled.
- (void)testManualFallbackShowsVirtualCards {
  // Create & save credit card enrolled in virtual card program.
  [AutofillAppInterface saveMaskedCreditCardEnrolledInVirtualCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Refresh the view by scrolling to the top as the virtual card and regular
  // card cells are otherwise superimposed. We don't think this issue is likely
  // to happen in production, but it's worth investigating further.
  // TODO(crbug.com/359542780): Remove when rendering issue is fixed.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Assert presence of virtual card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"Mastercard \nVirtual card")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down to show the CVC chip button for virtual cards.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 100)];
  [[EarlGrey selectElementWithMatcher:CvcChipButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down to show original card.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 200)];

  // Assert presence of original card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mastercard ")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view shows the CVC field for cards enrolled in
// CardInfoRetrieval.
- (void)testManualFallbackShowsCvcForCardInfoRetrievalEnrolledCard {
  // Create & save credit card enrolled in virtual card program.
  [AutofillAppInterface saveMaskedCreditCardEnrolledInCardInfoRetrieval];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Assert presence of the card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mastercard ")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down to show the CVC chip button.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:CvcChipButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view for credit cards shows a label for each
// button.
- (void)testManualFallbackShowsCardLabeledButtons {
  // Create & save local credit card.
  [AutofillAppInterface saveLocalCreditCardWithCvc];

  // Create & save masked credit card.
  [AutofillAppInterface saveMaskedCreditCard];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Scroll up to show the Card number chip button.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Assert presence of the server card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mastercard ")]
      assertWithMatcher:grey_notNil()];

  // Scroll down to show the CVC chip button.
  [[[EarlGrey selectElementWithMatcher:CvcChipButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Scroll down to show the local card.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 350)];

  // Assert card number label.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"Card number:"),
                                          grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert card number button.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                      kLocalNumberObfuscated),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert expiration date label.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                      @"Expiration date:"),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert expiration month button.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kLocalCardExpirationMonth),
                                          grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert expiration year button.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                      kLocalCardExpirationYear),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert card holder name label.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                      @"Name on card:"),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
  // Assert card holder name button.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                      kLocalCardHolder),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert CVC label.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"CVC:"),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Assert CVC button.
  [[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"123"),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

// Tests that the "Manage Payment Methods..." action works.
- (void)testManagePaymentMethodsActionOpensPaymentMethodSettings {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Manage Payment Methods..." action.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ManagePaymentMethodsMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsCreditCardMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Add Payment Method..." action works.
- (void)testAddPaymentMethodActionOpensAddPaymentMethodSettings {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  if (![ChromeEarlGrey isIPadIdiom]) {
    // Try to scroll on iPhone.
    [[EarlGrey
        selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }

  // Tap the "Add Payment Method..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::AddPaymentMethodMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:chrome_test_util::
                                                          AddCreditCardView()];
}

// Tests that the "Add Payment Method..." action works on OTR.
// TODO(crbug.com/462093327): Re-enable flaky test.
- (void)FLAKY_testOTRAddPaymentMethodActionOpensAddPaymentMethodSettings {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadURL];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Scroll if not iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey
        selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }

  // Tap the "Add Payment Method..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::AddPaymentMethodMatcher()]
      performAction:grey_tap()];

  // Verify the payment method settings opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// outside the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissCreditCardController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::CreditCardTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
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

  // Bring up the keyboard.
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
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that, after switching fields, the content size of the table view didn't
// grow.
// TODO(crbug.com/440045841): Re-enable when fixed.
- (void)DISABLED_testCreditCardControllerKeepsRightSize {
  // TODO(crbug.com/443204278): Fails on iOS 26 simulator.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the second element.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementCardNumber)];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
}

// Tests that the credit card View Controller stays on rotation.
- (void)testCreditCardControllerSupportsRotation {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];

  // Verify the credit card controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_OK)] performAction:grey_tap()];
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_OK)] performAction:grey_tap()];
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

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

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

// Tests that the overflow menu button is visible.
- (void)testOverflowMenuVisibility {
  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the overflow menu button is never visible in virtual card cells.
- (void)testOverflowMenuVisibilityForVirtualCards {
  // Create & save credit card enrolled in virtual card program.
  [AutofillAppInterface saveMaskedCreditCardEnrolledInVirtualCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Refresh the view by scrolling to the top as the virtual card and regular
  // card cells are otherwise superimposed. We don't think this issue is likely
  // to happen in production, but it's worth investigating further.
  // TODO(crbug.com/359542780): Remove when rendering issue is fixed.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  // Assert presence of virtual card.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"Mastercard \nVirtual card")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The overflow menu button should not be visible.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Edit" action of a local card's overflow menu button displays
// the card's details in edit mode.
- (void)testEditLocalCardFromOverflowMenu {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  // Save a  local card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // Verify that the card's details page is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillCreditCardEditTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Edit the name.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Test User")]
      performAction:grey_replaceText(@"New Name")];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];

  // TODO(crbug.com/332956674): Check that the updated suggestion is visible.
}

// Tests that the "Edit" action of a server card overflow menu button opens a
// new tab page.
- (void)testEditServerCardFromOverflowMenu {
  // Save a server card.
  [AutofillAppInterface saveMaskedCreditCard];

  [self loadURL];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  DismissPaymentBottomSheet();
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // Tapping on "Edit" should have opened the Payments web page. Verify that the
  // card's details page wasn't opened and that the card table view controller
  // is no longer visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillCreditCardEditTableView()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the "Show Details" action of the overflow menu button displays the
// card's details.
- (void)testShowCardDetailsFromOverflowMenu {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the overflow menu button and select the "Show Details" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuShowDetailsAction()]
      performAction:grey_tap()];

  // Verify that the card's details are visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillCreditCardEditTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the "Done" button to dismiss the view.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];

  // TODO(crbug.com/332956674): Check that the expanded view is still visible.
}

// Tests that tapping the "Autofill Form" button fills the payment form with
// the right data.
- (void)testAutofillFormButtonFillsForm {
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Save a card.
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:autofill::test::GetCreditCard()];

  // Verify that the acceptance of the card suggestion at index 0 was correctly
  // recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [AutofillAppInterface clearMockReauthenticationModule];
}

// Tests that the GPay icon is only visible when the card is a server card.
- (void)testGPayIconVisibility {
  // Save a local and a masked card.
  NSString* local_card_last_digits = [AutofillAppInterface saveLocalCreditCard];
  NSString* masked_card_last_digits =
      [AutofillAppInterface saveMaskedCreditCard];

  [self loadURL];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  DismissPaymentBottomSheet();
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the payment method manual fill view.
  OpenPaymentMethodManualFillView();

  // Scroll down to show the server card.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 40)];

  // Check that the GPay icon is visible in the masked card cell.
  [[EarlGrey selectElementWithMatcher:GPayIcon(masked_card_last_digits)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll down to show the local card.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 200)];

  // Check that the GPay icon is not visible in the local card cell.
  [[EarlGrey selectElementWithMatcher:GPayIcon(local_card_last_digits)]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Private

- (void)loadURL {
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];
}

- (void)verifyCreditCardButtonWithTitle:(NSString*)title
                        doesInjectValue:(NSString*)result {
  [AutofillAppInterface saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

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
