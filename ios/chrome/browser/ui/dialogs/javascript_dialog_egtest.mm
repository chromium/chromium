// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/dialogs/dialog_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::OKButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::WebViewMatcher;

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Body script for test page that shows an alert with kAlertMessage and returns
// kAlertResult.
const char kAlertURLPath[] = "/alert";
const char kAlertMessage[] = "This is a JavaScript alert.";
const char kAlertResult[] = "JAVASCRIPT ALERT WAS DISMISSED";
const char kAlertScriptBodyFormat[] = "alert(\"%s\");"
                                      "return \"%s\";";
std::string GetAlertScriptBody() {
  return base::StringPrintf(kAlertScriptBodyFormat, kAlertMessage,
                            kAlertResult);
}

// Body script for test page that shows a confirmation with kConfirmationMessage
// and returns kConfirmationResultOK or kConfirmationResultCancelled, depending
// on whether the OK or Cancel button was tapped.
const char kConfirmationURLPath[] = "/confirm";
const char kConfirmationMessage[] = "This is a JavaScript confirmation.";
const char kConfirmationResultOK[] = "Okay";
const char kConfirmationResultCancelled[] = "Cancelled";
const char kConfirmationScriptBodyFormat[] = "if (confirm(\"%s\") == true) {"
                                             "  return \"%s\";"
                                             "} else {"
                                             "  return \"%s\";"
                                             "}";
std::string GetConfirmationScriptBody() {
  return base::StringPrintf(kConfirmationScriptBodyFormat, kConfirmationMessage,
                            kConfirmationResultOK,
                            kConfirmationResultCancelled);
}

// Body script for test page that shows a prompt with kPromptMessage
// and returns kPromptTestUserInput or kPromptResultCancelled, depending on
// whether the OK or Cancel button was tapped.
const char kPromptURLPath[] = "/prompt";
const char kPromptMessage[] = "This is a JavaScript prompt.";
const char kPromptTestUserInput[] = "test";
const char kPromptResultCancelled[] = "Cancelled";
const char kPromptTestScriptBodyFormat[] = "var input = prompt(\"%s\");"
                                           "if (input != null) {"
                                           "  return input;"
                                           "} else {"
                                           "  return \"%s\";"
                                           "}";
std::string GetPromptScriptBody() {
  return base::StringPrintf(kPromptTestScriptBodyFormat, kPromptMessage,
                            kPromptResultCancelled);
}

// Script to inject that will show a JavaScript alert in a loop 20 times, then
// returns kAlertLoopFinishedText.
const char kAlertLoopURLPath[] = "/loop";
const char kAlertLoopFinishedText[] = "Loop Finished";
const char kAlertLoopScriptBodyFormat[] = "for (i = 0; i < 20; ++i) {"
                                          "  alert(\"ALERT TEXT\");"
                                          "}"
                                          "return \"%s\";";
std::string GetAlertLoopScriptBody() {
  return base::StringPrintf(kAlertLoopScriptBodyFormat, kAlertLoopFinishedText);
}

// HTTP server constants.

// The URL path for a test page which has a link to show JavaScript dialogs. The
// page calls the provided script with a timeout so that the JavaScript used to
// simulate the link tap can return while the dialogs are displayed.
const char kTestPageLinkID[] = "show-dialog";
const char kTestPageContentsFormat[] =
    "<!DOCTYPE html><html>"
    "  <body>"
    "    <script> function dialogScript() { %s } </script>"
    "    <script>"
    "      function runDialogScript() {"
    "        var result = dialogScript();"
    "        document.getElementById(\"dialog-result\").innerHTML = result;"
    "      }"
    "    </script>"
    "    <a onclick=\"setTimeout(runDialogScript, 0)\" id=\"%s\" "
    "       href=\"javascript:void(0);\">"
    "      Show Dialog."
    "    </a>"
    "    <p id=\"dialog-result\"></p>"
    "  </body>"
    "</html>";
std::string GetTestPageContents(std::string script_body) {
  return base::StringPrintf(kTestPageContentsFormat, script_body.c_str(),
                            kTestPageLinkID);
}
// The URL path for a page that shows an alert onload.
const char kOnLoadURLPath[] = "/onload";
const char kOnLoadContents[] =
    "<!DOCTYPE html><html><body onload=\"alert('alert')\"></body></html>";
// The URL path for a page with a link to kOnLoadURLPath.
const char kLinkPageURLPath[] = "/link";
const char kLinkPageContentsFormat[] =
    "<!DOCTYPE html><html><body><a id=\"%s\" href=\"%s\">%s</a></body></html>";
const char kLinkPageLinkText[] = "LINK TO ONLOAD ALERT PAGE";
const char kLinkID[] = "link-id";
std::string GetLinkPageContents(const GURL& on_load_url) {
  return base::StringPrintf(kLinkPageContentsFormat, kLinkID,
                            on_load_url.spec().c_str(), kLinkPageLinkText);
}

// Helper function that returns a text/html HttpResponse using |content|.
std::unique_ptr<net::test_server::HttpResponse> GetHttpResponseWithContent(
    const std::string& content) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(content);
  return std::move(http_response);
}

// net::EmbeddedTestServer handler for the test page.
std::unique_ptr<net::test_server::HttpResponse> LoadTestPage(
    std::string script_body,
    const net::test_server::HttpRequest& request) {
  return GetHttpResponseWithContent(GetTestPageContents(script_body));
}
// net::EmbeddedTestServer handler for kOnLoadURLPath.
std::unique_ptr<net::test_server::HttpResponse> LoadPageWithOnLoadAlert(
    const net::test_server::HttpRequest& request) {
  return GetHttpResponseWithContent(kOnLoadContents);
}
// net::EmbeddedTestServer handler for kLinkPageURLPath.
std::unique_ptr<net::test_server::HttpResponse> LoadPageWithLinkToOnLoadPage(
    ChromeTestCase* test_case,
    const net::test_server::HttpRequest& request) {
  GURL on_load_page_url = test_case.testServer->GetURL(kOnLoadURLPath);
  return GetHttpResponseWithContent(GetLinkPageContents(on_load_page_url));
}

// Assert that an alert with |alert_text| has been shown or hidden.
void WaitForAlertWithText(NSString* alert_text, bool visible) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> text_matcher =
        chrome_test_util::StaticTextWithAccessibilityLabel(alert_text);
    [[EarlGrey selectElementWithMatcher:text_matcher]
        assertWithMatcher:visible ? grey_notNil() : grey_nil()
                    error:&error];
    return !error;
  };
  NSString* error_text_format = visible
                                    ? @"Dialog with text was not shown: %@"
                                    : @"Dialog with text was not hidden: %@";
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             error_text_format, alert_text);
}

// Waits for a JavaScript dialog from |url| to be shown or hidden.
void WaitForJavaScriptDialog(const GURL& url, bool visible) {
  base::string16 URLString = url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  NSString* expectedTitle =
      l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, URLString);
  WaitForAlertWithText(expectedTitle, visible);
}

// Types |input| in the prompt.
void TypeInPrompt(NSString* input) {
  id<GREYMatcher> text_field_matcher = grey_allOf(
      grey_kindOfClass([UITextField class]),
      grey_accessibilityID(kJavaScriptDialogTextFieldAccessibiltyIdentifier),
      nil);
  [[EarlGrey selectElementWithMatcher:text_field_matcher]
      performAction:grey_typeText(input)];
}

void TapCancel() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
}

void TapSuppressDialogsButton() {
  id<GREYMatcher> suppress_dialogs_button =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  [[EarlGrey selectElementWithMatcher:suppress_dialogs_button]
      performAction:grey_tap()];
}

}  // namespace

@interface JavaScriptDialogTestCase : ChromeTestCase
@end

@implementation JavaScriptDialogTestCase

#pragma mark - ChromeTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kAlertURLPath,
      base::BindRepeating(&LoadTestPage, GetAlertScriptBody())));
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kConfirmationURLPath,
      base::BindRepeating(&LoadTestPage, GetConfirmationScriptBody())));
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kPromptURLPath,
      base::BindRepeating(&LoadTestPage, GetPromptScriptBody())));
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kAlertLoopURLPath,
      base::BindRepeating(&LoadTestPage, GetAlertLoopScriptBody())));
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kOnLoadURLPath,
      base::BindRepeating(&LoadPageWithOnLoadAlert)));
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kLinkPageURLPath,
      base::BindRepeating(&LoadPageWithLinkToOnLoadPage, self)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

- (void)tearDown {
  NSError* errorOK = nil;
  NSError* errorCancel = nil;

  // Dismiss JavaScript alert by tapping Cancel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()
              error:&errorCancel];
  // Dismiss JavaScript alert by tapping OK.
  id<GREYMatcher> OKButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_OK);
  [[EarlGrey selectElementWithMatcher:OKButton] performAction:grey_tap()
                                                        error:&errorOK];

  if (!errorOK || !errorCancel) {
    GREYFail(@"There are still alerts");
  }
  [super tearDown];
}

#pragma mark - Tests

// Tests that an alert is shown, and that the completion block is called.
- (void)testShowJavaScriptAlert {
  // Load the alert test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kAlertURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Check the message of the alert.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(@(kAlertMessage));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];

  // Tap the OK button to close the alert.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertResult];
}

// Tests that a confirmation dialog is shown, and that the completion block is
// called with the correct value when the OK buton is tapped.
- (void)testShowJavaScriptConfirmationOK {
  // Load the confirmation test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kConfirmationURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Check the message of the dialog.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(
          @(kConfirmationMessage));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kConfirmationResultOK];
}

// Tests that a confirmation dialog is shown, and that the completion block is
// called with the correct value when the Cancel buton is tapped.
- (void)testShowJavaScriptConfirmationCancelled {
  // Load the confirmation test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kConfirmationURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Check the message of the dialog.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(
          @(kConfirmationMessage));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kConfirmationResultCancelled];
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the OK buton is tapped.
- (void)testShowJavaScriptPromptOK {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  // Load the prompt test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kPromptURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Check the message of the dialog.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(@(kPromptMessage));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the html  to be reset to the input text.
  [ChromeEarlGrey waitForWebStateContainingText:kPromptTestUserInput];
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the Cancel buton is tapped.
- (void)testShowJavaScriptPromptCancelled {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  // Load the prompt test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kPromptURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Check the message of the dialog.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(@(kPromptMessage));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the Cancel button.
  TapCancel();
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the html  to be reset to the cancel text.
  [ChromeEarlGrey waitForWebStateContainingText:kPromptResultCancelled];
}

// Tests that JavaScript alerts that are shown in a loop can be suppressed.
- (void)testShowJavaScriptAlertLoop {
  // Load the alert test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kAlertLoopURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Tap the OK button to close the alert, then verify that the next alert in
  // the loop is shown.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  // Tap the suppress dialogs button.
  TapSuppressDialogsButton();
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for confirmation action sheet to be shown.
  NSString* alertLabel =
      l10n_util::GetNSString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
  WaitForAlertWithText(alertLabel, /*visible=*/true);

  // Tap the suppress dialogs confirmation button.
  TapSuppressDialogsButton();
  WaitForAlertWithText(alertLabel, /*visible=*/false);

  // Wait for the html  to be reset to the loop finished text.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertLoopFinishedText];
}

// Tests to ensure crbug.com/658260 does not regress.
// Tests that if an alert should be called when settings are displays, the alert
// waits for the dismiss of the settings.
- (void)testShowJavaScriptBehindSettings {
  // Load the alert test page.
  const GURL kURL = self.testServer->GetURL(kAlertURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];

  // Show settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_IOS_SETTINGS_TITLE)),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the link to trigger the dialog.
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];

  // Make sure the alert is not present.
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Close the settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Make sure the alert is present.
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertResult];
}

// Tests that an alert is presented after displaying the share menu.
- (void)testShowJavaScriptAfterShareMenu {
  // TODO(crbug.com/747622): re-enable this test once earl grey can interact
  // with the share menu.
  EARL_GREY_TEST_DISABLED(@"Disabled until EG can use share menu.");

  // Load the blank test page.
  const GURL kURL = self.testServer->GetURL(kAlertURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];

  [ChromeEarlGreyUI openShareMenu];

  // Copy URL, dismissing the share menu.
  id<GREYMatcher> printButton =
      grey_allOf(grey_accessibilityLabel(@"Copy"),
                 grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
  [[EarlGrey selectElementWithMatcher:printButton] performAction:grey_tap()];

  // Show an alert and assert it is present.
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, /*visible=*/true);

  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, /*visible=*/false);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertResult];
}

// Tests that an alert is presented after a new tab animation is finished.
- (void)testShowJavaScriptAfterNewTabAnimation {
  // TODO(crbug.com/1007986) Test flaky on iOS13.
  if (@available(iOS 13, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS13.");
  }

  // Load the test page with a link to kOnLoadAlertURL and long tap on the link.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLinkPageURLPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLinkPageLinkText];

  // TODO(crbug.com/712358): Use method LongPressElementAndTapOnButton once
  // it is moved out of context_menu_egtests.mm and into a shared location.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkID],
                        true /* menu should appear */)];

  // Tap on the "Open In New Tab" button.
  id<GREYMatcher> newTabMatcher = ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB));
  [[EarlGrey selectElementWithMatcher:newTabMatcher] performAction:grey_tap()];

  // This test case requires that a dialog is presented in the onload event so
  // that the DialogPresenter attempts to display during a new tab animation.
  // Because presenting a dialog halts the JavaScript execution on the page,
  // this prevents the page loaded event from being received until the alert is
  // closed.  On iPad, this means that there is a loading indicator that
  // continues to animate until the dialog is closed.  Disabling EarlGrey
  // synchronization code for iPad allows the test to detect and dismiss the
  // dialog while this animation is occurring.
  {
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();
    if (![ChromeEarlGrey isIPadIdiom]) {
      disabler.reset();
    }

    // Wait for the alert to be shown.
    GURL kOnLoadURL = self.testServer->GetURL(kOnLoadURLPath);
    WaitForJavaScriptDialog(kOnLoadURL, /*visible=*/true);

    // Verify that the omnibox shows the correct URL when the dialog is visible.
    std::string title =
        base::SysNSStringToUTF8([ChromeEarlGrey displayTitleForURL:kOnLoadURL]);
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(title)]
        assertWithMatcher:grey_notNil()];

    [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  }
}

@end
