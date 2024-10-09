// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
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
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
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
const char kPasswordFieldID[] = "pw";

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

// Matcher for the overflow menu button shown in the cells.
id<GREYMatcher> OverflowMenuButton() {
  return grey_allOf(
      grey_accessibilityID(
          manual_fill::kExpandedManualFillPlusAddressOverflowMenuID),
      grey_interactable(), nullptr);
}

// Matcher for the "Manage" action made available by the overflow menu
// button.
id<GREYMatcher> OverflowMenuManageAction() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_CONTENT_CONTEXT_OPENMANAGEINNEWTAB),
                    grey_interactable(), nullptr);
}

// Returns a matcher for the button to dismiss select plus address in manual
// fallback.
id<GREYMatcher> PlusAddressSelectDoneMatcher() {
  return grey_accessibilityID(
      manual_fill::kPlusAddressDoneButtonAccessibilityIdentifier);
}

// Returns a matcher for the plus address search bar in manual fallback.
id<GREYMatcher> PlusAddressSelectSearchBarMatcher() {
  return grey_accessibilityID(
      manual_fill::kPlusAddressSearchBarAccessibilityIdentifier);
}

// Returns a matcher for the select plus address action.
id<GREYMatcher> PlusAddressSelectActionMatcher() {
  return grey_accessibilityID(
      manual_fill::kSelectPlusAddressAccessibilityIdentifier);
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
  [AutofillAppInterface clearProfilesStore];
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

// Verify that the `fieldName` has been filled with `value`.
- (void)verifyFieldHasBeenFilledWithValue:(std::u16string)value {
  NSString* fillCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kNameFieldID, base::SysUTF16ToNSString(value)];
  [ChromeEarlGrey waitForJavaScriptCondition:fillCondition];
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
      selectElementWithMatcher:manual_fill::ChipButton(
                                   plus_addresses::test::kFakePlusAddressU16)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Switch over to passwords.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(
                                   plus_addresses::test::kFakePlusAddressU16)]
      performAction:grey_tap()];

  [self verifyFieldHasBeenFilledWithValue:plus_addresses::test::
                                              kFakePlusAddressU16];
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

  // Take note of how many tabs are open before clicking the manage,
  // which should simply open a new tab.
  NSUInteger oldRegularTabCount = [ChromeEarlGrey mainTabCount];
  NSUInteger oldIncognitoTabCount = [ChromeEarlGrey incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:managePlusAddressMatcher]
      performAction:grey_tap()];

  // A new tab should open after tapping the manage action.
  [ChromeEarlGrey waitForMainTabCount:oldRegularTabCount + 1];
  [ChromeEarlGrey waitForIncognitoTabCount:oldIncognitoTabCount];
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

  [self verifyFieldHasBeenFilledWithValue:plus_addresses::test::
                                              kFakePlusAddressU16];
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

// Tests that tapping on the select plus address action shows a sheet with the
// list of all plus addresses from the address manual fill view.
- (void)testSelectPlusAddressActionFromAddressFillView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setPlusAddressFillingEnabled:YES];
  [PlusAddressAppInterface addPlusAddressProfile];

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:PlusAddressSelectActionMatcher()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(u"plus+foo@plus.plus")]
      performAction:grey_tap()];

  [self verifyFieldHasBeenFilledWithValue:u"plus+foo@plus.plus"];
}

// Tests that tapping on the select plus address action shows a sheet with the
// list of all plus addresses from the password manual fill view.
- (void)testSelectPlusAddressActionFromPasswordFillView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setPlusAddressFillingEnabled:YES];
  [PlusAddressAppInterface addPlusAddressProfile];

  // Load form.
  LoadForm(self.testServer, ManualFillDataType::kPassword);

  // Tap on the provided field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kPasswordFieldID)];

  // Tap the button that'll open the password manual fill view.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::PasswordManualFillViewButton()]
      performAction:grey_tap()];

  // Confirm that the expanded manual fill view is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::ExpandedManualFillView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:PlusAddressSelectActionMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(u"plus+foo@plus.plus")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:PlusAddressSelectDoneMatcher()]
      performAction:grey_tap()];
}

// Tests the search functionality in the select plus address sheet view.
- (void)testSearchPlusAddress {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setPlusAddressFillingEnabled:YES];
  [PlusAddressAppInterface addPlusAddressProfile];

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:PlusAddressSelectActionMatcher()]
      performAction:grey_tap()];

  // Tap the search option.
  [[EarlGrey selectElementWithMatcher:PlusAddressSelectSearchBarMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PlusAddressSelectSearchBarMatcher()]
      performAction:grey_replaceText(@"example1")];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(u"plus+foo@plus.plus")]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:PlusAddressSelectSearchBarMatcher()]
      performAction:grey_replaceText(@"foo.com")];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(u"plus+foo@plus.plus")]
      performAction:grey_tap()];

  [self verifyFieldHasBeenFilledWithValue:u"plus+foo@plus.plus"];
}

// Tests that for the plus address manual fallback suggestion, in the overflow
// menu, there is an option to manage the plus address.
- (void)testOverflowMenuManageActionInAddressManualFillMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  // Open the expanded manual fill view for an address field.
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::ChipButton(
                                   plus_addresses::test::kFakePlusAddressU16)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the overflow menu button.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
      performAction:grey_tap()];

  // Take note of how many tabs are open before clicking the manage,
  // which should simply open a new tab.
  NSUInteger oldRegularTabCount = [ChromeEarlGrey mainTabCount];
  NSUInteger oldIncognitoTabCount = [ChromeEarlGrey incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OverflowMenuManageAction()]
      performAction:grey_tap()];

  // A new tab should open after tapping the manage action.
  [ChromeEarlGrey waitForMainTabCount:oldRegularTabCount + 1];
  [ChromeEarlGrey waitForIncognitoTabCount:oldIncognitoTabCount];
}

// Tests that the "Manage" action in the overflow menu is displayed in the
// select plus address view.
- (void)testOverflowMenuManageActionInSelectPlusAddressView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails for iPad");
  }

  [PlusAddressAppInterface setPlusAddressFillingEnabled:YES];
  [PlusAddressAppInterface addPlusAddressProfile];

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];
  id<GREYMatcher> selectPlusAddressMatcher = grey_accessibilityID(
      manual_fill::kSelectPlusAddressAccessibilityIdentifier);

  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [[EarlGrey selectElementWithMatcher:selectPlusAddressMatcher]
      performAction:grey_tap()];

  // Tap the overflow menu button.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
      performAction:grey_tap()];

  // Take note of how many tabs are open before clicking the manage,
  // which should simply open a new tab.
  NSUInteger oldRegularTabCount = [ChromeEarlGrey mainTabCount];
  NSUInteger oldIncognitoTabCount = [ChromeEarlGrey incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OverflowMenuManageAction()]
      performAction:grey_tap()];

  // A new tab should open after tapping the manage action.
  [ChromeEarlGrey waitForMainTabCount:oldRegularTabCount + 1];
  [ChromeEarlGrey waitForIncognitoTabCount:oldIncognitoTabCount];
}

@end
