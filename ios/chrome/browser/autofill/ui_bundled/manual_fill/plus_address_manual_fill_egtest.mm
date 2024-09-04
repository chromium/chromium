// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/plus_addresses/fake_plus_address_service.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/plus_address_test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;
using net::test_server::EmbeddedTestServer;

namespace {

constexpr char kAddressFormURL[] = "/profile_form.html";
constexpr char kPasswordFormURL[] = "/simple_login_form.html";

const char kNameFieldID[] = "name";

// Loads a form depending on the desired `data_type`.
void LoadForm(EmbeddedTestServer* test_server, ManualFillDataType data_type) {
  std::string_view form_url;
  std::string form_text;
  switch (data_type) {
    case ManualFillDataType::kPassword:
      form_url = kPasswordFormURL;
      form_text = "Login form.";
      break;
    case ManualFillDataType::kAddress:
      form_url = kAddressFormURL;
      form_text = "Profile form";
      break;
    case ManualFillDataType::kOther:
    case ManualFillDataType::kPaymentMethod:
      NOTREACHED();
  }

  [ChromeEarlGrey loadURL:test_server->GetURL(form_url)];
  [ChromeEarlGrey waitForWebStateContainingText:form_text];
}

}  // namespace

// Test case for the plus address manual fill view.
@interface PlusAddressManualFillTestCase : WebHttpServerChromeTestCase

@end

@implementation PlusAddressManualFillTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  std::string fakeLocalUrl =
      base::EscapeQueryParamValue("chrome://version", /*use_plus=*/false);
  config.features_enabled_and_params.push_back(
      {plus_addresses::features::kPlusAddressesEnabled,
       {{
           {"server-url", {fakeLocalUrl}},
           {"manage-url", {fakeLocalUrl}},
       }}});

  // Enable the Keyboard Accessory Upgrade feature.
  config.features_enabled_and_params.push_back(
      {kIOSKeyboardAccessoryUpgrade, {}});
  config.features_enabled_and_params.push_back(
      {plus_addresses::features::kPlusAddressIOSManualFallbackEnabled, {}});
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kAddFakePlusAddressService);

  return config;
}

- (void)setUp {
  [super setUp];

  // TODO(crbug.com/327838014): Fix and enable tests for iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [AutofillAppInterface saveExampleAccountProfile];
}

- (void)tearDown {
  [SigninEarlGrey signOut];

  [super tearDown];
}

// Opens the expanded manual fill view for a given `dataType`. `fieldToFill` is
// the ID of the form field that should be focused prior to opening the expanded
// manual fill view.
- (void)openExpandedManualFillViewForDataType:(ManualFillDataType)dataType
                                  fieldToFill:(std::string)fieldToFill {
  // Load form.
  LoadForm(self.testServer, dataType);

  // Tap on the provided field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(fieldToFill)];

  // Open the expanded manual fill view.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::KeyboardAccessoryManualFillButton()];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::KeyboardAccessoryManualFillButton()]
      performAction:grey_tap()];

  // Confirm that the expanded manual fill view is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::ExpandedManualFillView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the plus address fallback is shown in the address and the
// password segment.
- (void)testPlusAddressFallback {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  // Open the expanded manual fill view for an address field.
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  [[EarlGrey
      selectElementWithMatcher:
          manual_fill::ChipButton(
              plus_addresses::FakePlusAddressService::kFakePlusAddress16)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Switch over to passwords.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          manual_fill::ChipButton(
              plus_addresses::FakePlusAddressService::kFakePlusAddress16)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the plus address manage action are shown in the address and
// password segments.
- (void)testPlusAddressManageAction {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  id<GREYMatcher> managePlusAddressMatcher = grey_accessibilityID(
      manual_fill::kManagePlusAddressAccessibilityIdentifier);

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:managePlusAddressMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Switch over to passwords.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:managePlusAddressMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping on the create plus address action in the address manual
// fill view opens up the bottomsheet to create one.
- (void)testPlusAddressCreateActionFromAddressView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setShouldOfferPlusAddressCreation:YES];
  [PlusAddressAppInterface setShouldReturnNoAffiliatedPlusProfiles:YES];

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  id<GREYMatcher> createPlusAddressMatcher = grey_accessibilityID(
      manual_fill::kCreatePlusAddressAccessibilityIdentifier);

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:createPlusAddressMatcher]
      performAction:grey_tap()];

  id<GREYMatcher> createPlusAddressBottomSheetButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_OK_TEXT_IOS);
  [[EarlGrey selectElementWithMatcher:createPlusAddressBottomSheetButton]
      performAction:grey_tap()];

  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kNameFieldID, @"plus+remote@plus.plus"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Tests that tapping on the create plus address action in the password manual
// fill view opens up the bottomsheet to create one.
- (void)testPlusAddressCreateActionFromPasswordView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setShouldOfferPlusAddressCreation:YES];
  [PlusAddressAppInterface setShouldReturnNoAffiliatedPlusProfiles:YES];

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  id<GREYMatcher> createPlusAddressMatcher = grey_accessibilityID(
      manual_fill::kCreatePlusAddressAccessibilityIdentifier);

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Switch over to passwords.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:createPlusAddressMatcher]
      performAction:grey_tap()];

  id<GREYMatcher> createPlusAddressBottomSheetCancelButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT);
  [[EarlGrey selectElementWithMatcher:createPlusAddressBottomSheetCancelButton]
      performAction:grey_tap()];
}

@end
