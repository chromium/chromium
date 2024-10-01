// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/authentication_egtest_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForDownloadTimeout;

// Path to the autofill test pages.
const char kAutofillTestPagesDirectory[] = "components/test/data/autofill";

// Url of the test page with a credit card form and form filling buttons.
const char kCreditCardUploadUrl[] =
    "/credit_card_upload_form_address_and_cc.html";

// The id of the form filling button on the credit card upload page.
const char kFillFormId[] = "fill_form";

// The submit button on the form.
const char kSubmitFormId[] = "submit";

// Url for get save card details on the payments server. Tests inject a response
// instead of calling this endpoint.
NSString* const kGetSaveCardDetailsUrl =
    @"https://payments.google.com/payments/apis/chromepaymentsservice/"
    @"getdetailsforsavecard";

// A successful response to the get save card details request.
NSString* const kGetSaveCardDetailsResponse =
    @"{\"legal_message\":{\"line\":[{\"template\":\"See terms "
    @"{0}.\",\"template_parameter\":[{\"display_text\":\"Terms of "
    @"Service\",\"url\":\"https://example.test/"
    @"terms\"}]}]},\"context_token\":\"fake_context_token\",\"supported_card_"
    @"bin_ranges_string\":\"1,2,3,4,5,6,7,8,9,0\"}";

// Url for the save card endpoint on the payments server. Tests inject a
// response instead of calling this endpoint.
NSString* const kSaveCardUrl =
    @"https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    @"savecard?s7e_suffix=chromewallet";

// A successful response for the save card endpoint that includes virtual card
// metadata.
NSString* const kSaveCardResponseWithVirtualCardMetadata =
    @"{\"virtual_card_metadata\":{\"status\":\"ENROLLMENT_ELIGIBLE\",\"virtual_"
    @"card_enrollment_data\":{\"google_legal_message\":{\"line\":[{"
    @"\"template\":\"See terms "
    @"{0}.\",\"template_parameter\":[{\"display_text\":\"Privacy "
    @"Notice\",\"url\":\"https://example.test/"
    @"privacy_notice\"}]}]},\"external_legal_message\":{\"line\":[{"
    @"\"template\":\"See issuer terms "
    @"{0}.\",\"template_parameter\":[{\"display_text\":\"Issuer\u0027s "
    @"Terms\",\"url\":\"https://example.test/"
    @"issuer_terms\"}]}]},\"context_token\":\"fake_context_token\"}},"
    @"\"instrument_id\":\"1\"}";

// Url for the virtual card enrollment endpoint on the payments server. Tests
// inject a response instead of calling this endpoint.
NSString* const kVirtualCardEnrollUrl =
    @"https://payments.google.com/payments/apis/virtualcardservice/enroll";

// A successful response for the virtual card enrollment endpoint.
NSString* const kVirtualCardEnrollResponseSuccess =
    @"{\"enroll_result\":\"ENROLL_SUCCESS\"}";

id<GREYMatcher> VirtualCardEnrollmentTitle() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL));
}

id<GREYMatcher> VirtualCardEnrollmentAcceptButton() {
  return testing::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL));
}

id<GREYMatcher> VirtualCardEnrollmentSkipButton() {
  return testing::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP));
}

@interface VirtualCardEnrollmentBottomSheetEgTest : ChromeTestCase
@end

@implementation VirtualCardEnrollmentBottomSheetEgTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self
          isRunningTest:@selector
          (testVirtualCardEnrollmentShowsLoadingAndConfirmationAfterAcceptPushed
              )]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  } else if ([self isRunningTest:@selector
                   (testVirtualCardEnrollmentDismissesAfterAcceptPushed)]) {
    config.features_disabled.push_back(
        autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface setUpFakeCreditCardServer];
  [AutofillAppInterface clearVirtualCardEnrollmentStrikes];
  [self setUpServer];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
}

- (void)setUpServer {
  self.testServer->ServeFilesFromSourceDirectory(kAutofillTestPagesDirectory);
  GREYAssertTrue(self.testServer->Start(), @"Failed to start test server.");
}

- (void)tearDown {
  [AutofillAppInterface clearAllServerDataForTesting];
  [AutofillAppInterface clearVirtualCardEnrollmentStrikes];
  [AutofillAppInterface tearDownFakeCreditCardServer];
  [super tearDown];
}

- (void)showVirtualCardEnrollmentBottomSheet {
  // Load the test page with a credit card form.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kCreditCardUploadUrl)];

  // Inject a response from the payments server for get save card details.
  [AutofillAppInterface setPaymentsResponse:kGetSaveCardDetailsResponse
                                 forRequest:kGetSaveCardDetailsUrl
                              withErrorCode:net::HTTP_OK];

  // Fill the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFillFormId)];

  // Inject a response from the payments server when saving the card.
  [AutofillAppInterface
      setPaymentsResponse:kSaveCardResponseWithVirtualCardMetadata
               forRequest:kSaveCardUrl
            withErrorCode:net::HTTP_OK];

  // Expect card to be uploaded and a response to be received.
  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kSubmitFormId)];

  // Wait for upload and get upload details.
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Did not call upload save or get upload details response.");

  // Push the save button on the card.
  id<GREYMatcher> overlaySaveButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_AUTOFILL_SAVE_ELLIPSIS);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:overlaySaveButton];
  [[EarlGrey selectElementWithMatcher:overlaySaveButton]
      performAction:grey_tap()];

  // Push the save button on the modal.
  id<GREYMatcher> modalSaveButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_AUTOFILL_SAVE_CARD);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:modalSaveButton];
  [[EarlGrey selectElementWithMatcher:modalSaveButton]
      performAction:grey_tap()];

  // Inject risk data required for the card upload request to be initiated.
  [AutofillAppInterface setPaymentsRiskData:@"Fake risk data for tests"];

  // Wait for the virtual card enrollment bottomsheet to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:VirtualCardEnrollmentTitle()];
}

- (void)testVirtualCardEnrollmentDismissesAfterSkipPushed {
  [self showVirtualCardEnrollmentBottomSheet];

  // Assert the header trait is set on the header label.
  [[EarlGrey selectElementWithMatcher:VirtualCardEnrollmentTitle()]
      assertWithMatcher:grey_allOf(
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            grey_sufficientlyVisible(), nil)];

  // Push the skip button on the virtual card enrollment bottom sheet.
  [[EarlGrey
      selectElementWithMatcher:
          testing::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
              IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP))]
      performAction:grey_tap()];

  // Assert the virtual card enrollment bottom sheet has been dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:VirtualCardEnrollmentTitle()];
}

- (void)testVirtualCardEnrollmentDismissesAfterAcceptPushed {
  [self showVirtualCardEnrollmentBottomSheet];

  // Push the accept button on the virtual card enrollment bottom sheet.
  [[EarlGrey selectElementWithMatcher:VirtualCardEnrollmentAcceptButton()]
      performAction:grey_tap()];

  // Assert the virtual card enrollment bottom sheet has been dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:VirtualCardEnrollmentTitle()];
}

- (void)testVirtualCardEnrollmentShowsLoadingAndConfirmationAfterAcceptPushed {
  [self showVirtualCardEnrollmentBottomSheet];

  // Avoid immediately failing due to missing access token.
  [AutofillAppInterface setAccessToken];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Assert the logo has an accessibility label set.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME))]
      assertWithMatcher:grey_sufficientlyVisible()];
#endif

  // Push the accept button on the virtual card enrollment bottom sheet.
  [[EarlGrey selectElementWithMatcher:VirtualCardEnrollmentAcceptButton()]
      performAction:grey_tap()];

  // Assert an activity indicator view is being shown in the loading state.
  id<GREYMatcher> activityIndicatorView =
      grey_kindOfClassName(@"UIActivityIndicatorView");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:activityIndicatorView];
  [[[EarlGrey selectElementWithMatcher:activityIndicatorView]
      inRoot:grey_accessibilityID(
                 kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert the primary action button is disabled.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      assertWithMatcher:
          grey_allOf(
              grey_not(grey_enabled()),
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_LOADING_THROBBER_ACCESSIBLE_NAME)),
              nil)];

  // Assert the secondary action button is disabled.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertSecondaryActionAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Inject a successful enrollment response from the payments server.
  [AutofillAppInterface setPaymentsResponse:kVirtualCardEnrollResponseSuccess
                                 forRequest:kVirtualCardEnrollUrl
                              withErrorCode:net::HTTP_OK];

  // Assert the primary action button is still disabled.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Assert the primary action button contains the checkmark symbol.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kConfirmationAlertCheckmarkSymbolIdentifier)]
      inRoot:grey_accessibilityID(
                 kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
