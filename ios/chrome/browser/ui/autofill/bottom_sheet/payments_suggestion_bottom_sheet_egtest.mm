// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util_mac.h"

#import "ios/chrome/browser/shared/public/features/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFormCardName[] = "CCName";

}  // namespace

@interface PaymentsSuggestionBottomSheetEGTest : ChromeTestCase
@end

@implementation PaymentsSuggestionBottomSheetEGTest {
  // Last digits of the credit card
  NSString* _lastDigits;
}

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [AutofillAppInterface clearCreditCardStore];
  _lastDigits = [AutofillAppInterface saveLocalCreditCard];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSPaymentsBottomSheet);
  return config;
}

id<GREYMatcher> ContinueButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE));
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadPaymentsPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/credit_card.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];
}

#pragma mark - Tests

- (void)testOpenPaymentsBottomSheetUseCreditCard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];
}

- (void)testOpenPaymentsBottomSheetAfterLongPress {
  [self loadPaymentsPage];

  // Open the Payments Bottom Sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  // Long press to open context menu.
  id<GREYMatcher> creditCardEntry = grey_text(_lastDigits);

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabel(
                  l10n_util::GetNSString(
                      IDS_IOS_PAYMENT_BOTTOM_SHEET_MANAGE_PAYMENT_METHODS)),
              grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Close the context menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Try to open the Payments Bottom Sheet again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Make sure the bottom sheet re-opens.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];
}

@end
