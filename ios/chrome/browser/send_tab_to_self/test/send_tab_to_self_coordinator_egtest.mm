// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/send_tab_to_self/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const char kPageText[] =
    "This is a long and unique text that should be easy to generate a text "
    "fragment for without any ambiguity.";
const char kPageHtml[] =
    "<html><body>"
    "<div style='height: 100px; width: 100px; position: absolute; top: 50%; "
    "left: 50%; transform: translate(-50%, -50%);'>"
    "  <p id='target'>"
    "    This is a long and unique text that should be easy to generate a text "
    "fragment for without any ambiguity."
    "  </p>"
    "</div>"
    "</body></html>";

const char kScrollPageHtml[] =
    "<html><body>"
    "<div style='height: 2000px;'></div>"
    "  <p id='target'>"
    "    This is a long and unique text that should be easy to generate a text "
    "fragment for without any ambiguity."
    "  </p>"
    "</body></html>";

NSString* const kTargetDeviceName = @"My other device";
NSString* const kSendTabToSelfModalCancelButtonId =
    @"kSendTabToSelfModalCancelButton";

std::unique_ptr<net::test_server::HttpResponse> RespondWithConstantPage(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(request.relative_url == "/scroll" ? kScrollPageHtml
                                                               : kPageHtml);
  return http_response;
}
}  // namespace

@interface SendTabToSelfCoordinatorTestCase : ChromeTestCase
@end

@implementation SendTabToSelfCoordinatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateScrollPosition);
  return config;
}

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&RespondWithConstantPage));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the entry point button is shown to a signed out user, even if
// there are no device-level accounts.
- (void)testShowButtonIfSignedOutAndNoDeviceAccount {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:sendTabToSelf];
}

- (void)testShowPromoIfSignedOutAndHasDeviceAccount {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];

  // Confirm the promo.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConsistencySigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The device list should be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];
}

- (void)testShowMessageIfSignedInAndNoTargetDevice {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL)),
                     grey_userInteractionEnabled(), nil)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testShowDevicePickerIfSignedInAndHasTargetDevice {
  // Setting a recent timestamp here is necessary, otherwise the device will be
  // considered expired and won't be displayed.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testSendTabToSelfAndVerifySnackbar {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
      performAction:grey_tap()];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Tap the device in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kTargetDeviceName)]
      performAction:grey_tap()];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Wait for and verify the snackbar message.
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(kTargetDeviceName));
  id<GREYMatcher> snackbarMatcher = grey_allOf(
      chrome_test_util::SnackbarViewMatcher(),
      grey_descendant(grey_accessibilityLabel(snackbarMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];

  // Verify that the text fragment was successfully captured and attached to the
  // STTS entry in the model.
  NSString* urlString =
      base::SysUTF8ToNSString(self.testServer->GetURL("/scroll").spec());
  NSString* textFragment =
      [ChromeEarlGrey textFragmentForSendTabToSelfEntryWithURL:urlString];
  GREYAssertTrue(
      [textFragment caseInsensitiveCompare:base::SysUTF8ToNSString(
                                               kPageText)] == NSOrderedSame,
      @"Text fragment should be captured. Expected '%s' (case-insensitive) but "
      @"got %@",
      kPageText, textFragment);
}

// Tests that a text fragment is correctly consumed and scrolls the page
// when passed internally during an OpenNewTabCommand, without highlighting.
- (void)testRestoreScrollPosition {
  NSString* urlString =
      base::SysUTF8ToNSString(self.testServer->GetURL("/scroll").spec());

  // Use the known text fragment for the page content.
  NSString* textFragment = @"This%20is%20a%20long,without%20any%20ambiguity.";

  // Open the new tab with the text fragment.
  [ChromeEarlGrey openNewTabWithURL:urlString textFragment:textFragment];

  // Wait for the new tab to load and the fragment to be applied.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify that the page has scrolled down to the fragment.
  NSString* checkScrollJS = @"window.scrollY > 0;";
  [ChromeEarlGrey waitForJavaScriptCondition:checkScrollJS];

  // Verify that the polyfill did not leave any highlight marks.
  NSString* checkHighlightJS =
      @"document.getElementsByTagName('mark').length === 0;";
  BOOL noHighlight =
      [ChromeEarlGrey evaluateJavaScript:checkHighlightJS].GetBool();
  GREYAssertTrue(noHighlight, @"Text fragment should not be highlighted.");
}

// Tests that an invalid text fragment is safely ignored and doesn't crash or
// highlight.
- (void)testRestoreScrollPositionInvalidFragment {
  NSString* urlString =
      base::SysUTF8ToNSString(self.testServer->GetURL("/scroll").spec());

  // Use an invalid text fragment.
  NSString* textFragment = @"InvalidFragmentThatDoesNotMatchAnything";

  // Open the new tab with the text fragment.
  [ChromeEarlGrey openNewTabWithURL:urlString textFragment:textFragment];

  // Wait for the new tab to load.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify that the page has NOT scrolled down. We wait for a short duration
  // to ensure any pending async scrolls do not occur.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  NSString* checkScrollJS = @"window.scrollY === 0;";
  BOOL hasNotScrolled =
      [ChromeEarlGrey evaluateJavaScript:checkScrollJS].GetBool();
  GREYAssertTrue(hasNotScrolled, @"Page should not have scrolled.");

  // Verify that the polyfill did not leave any highlight marks.
  NSString* checkHighlightJS =
      @"document.getElementsByTagName('mark').length === 0;";
  BOOL noHighlight =
      [ChromeEarlGrey evaluateJavaScript:checkHighlightJS].GetBool();
  GREYAssertTrue(noHighlight, @"Text fragment should not be highlighted.");
}

// Tests that an empty text fragment is safely ignored.
- (void)testRestoreScrollPositionEmptyFragment {
  NSString* urlString =
      base::SysUTF8ToNSString(self.testServer->GetURL("/scroll").spec());

  // Use an empty text fragment.
  NSString* textFragment = @"";

  // Open the new tab with the text fragment.
  [ChromeEarlGrey openNewTabWithURL:urlString textFragment:textFragment];

  // Wait for the new tab to load.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify that the page has NOT scrolled down. We wait for a short duration
  // to ensure any pending async scrolls do not occur.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  NSString* checkScrollJS = @"window.scrollY === 0;";
  BOOL hasNotScrolled =
      [ChromeEarlGrey evaluateJavaScript:checkScrollJS].GetBool();
  GREYAssertTrue(hasNotScrolled,
                 @"Page should not have scrolled for empty fragment.");
}

@end
