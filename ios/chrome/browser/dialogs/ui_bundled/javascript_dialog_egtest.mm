// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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
const char kAlertLoopMessage[] = "This is a looped alert.";
const char kAlertLoopFinishedText[] = "Loop Finished";
const char kAlertLoopScriptBodyFormat[] = "for (i = 0; i < 20; ++i) {"
                                          "  alert(\"%s\");"
                                          "}"
                                          "return \"%s\";";
std::string GetAlertLoopScriptBody() {
  return base::StringPrintf(kAlertLoopScriptBodyFormat, kAlertLoopMessage,
                            kAlertLoopFinishedText);
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
const char kOnLoadAlertMessage[] = "onload Alert";
const char kOnLoadContentsFormat[] =
    "<!DOCTYPE html><html><body onload=\"alert('%s')\"></body></html>";
std::string GetOnLoadPageContents() {
  return base::StringPrintf(kOnLoadContentsFormat, kOnLoadAlertMessage);
}
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

// Helper function that returns a text/html HttpResponse using `content`.
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
  return GetHttpResponseWithContent(GetOnLoadPageContents());
}
// net::EmbeddedTestServer handler for kLinkPageURLPath.
std::unique_ptr<net::test_server::HttpResponse> LoadPageWithLinkToOnLoadPage(
    ChromeTestCase* test_case,
    const net::test_server::HttpRequest& request) {
  GURL on_load_page_url = test_case.testServer->GetURL(kOnLoadURLPath);
  return GetHttpResponseWithContent(GetLinkPageContents(on_load_page_url));
}

// Waits for a JavaScript dialog from `url` with `message` to be shown or
// hidden.
void WaitForJavaScriptDialog(const GURL& url,
                             const char* message,
                             bool visible,
                             bool is_main_frame) {
  // Wait for the JavaScript dialog identifier.
  id<GREYMatcher> visibility_matcher = visible ? grey_notNil() : grey_nil();
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> dialog_matcher =
        grey_accessibilityID(kJavaScriptDialogAccessibilityIdentifier);
    [[EarlGrey selectElementWithMatcher:dialog_matcher]
        assertWithMatcher:visibility_matcher
                    error:&error];
    return !error;
  };
  NSString* error_text = visible ? @"JavaScript dialog was not shown."
                                 : @"JavaScript dialog was not hidden.";
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             error_text);

  // Check the title.  Non-modal main-frame dialogs do not have a title label.
  if (!is_main_frame) {
    std::u16string url_string = url_formatter::FormatUrlForSecurityDisplay(
        url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    NSString* expected_title =
        l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, url_string);
    id<GREYMatcher> title_matcher =
        chrome_test_util::StaticTextWithAccessibilityLabel(expected_title);
    [[EarlGrey selectElementWithMatcher:title_matcher]
        assertWithMatcher:visibility_matcher];
  }

  // Check the message.
  id<GREYMatcher> message_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabel(@(message));
  [[EarlGrey selectElementWithMatcher:message_matcher]
      assertWithMatcher:visibility_matcher];
}

// Types `input` in the prompt.
void TypeInPrompt(NSString* input) {
  id<GREYMatcher> text_field_matcher = grey_allOf(
      grey_kindOfClass([UITextField class]),
      grey_accessibilityID(kJavaScriptDialogTextFieldAccessibilityIdentifier),
      nil);
  [[EarlGrey selectElementWithMatcher:text_field_matcher]
      performAction:grey_replaceText(input)];
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
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Tap the OK button to close the alert.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

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
  WaitForJavaScriptDialog(kURL, kConfirmationMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kConfirmationMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

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
  WaitForJavaScriptDialog(kURL, kConfirmationMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];

  WaitForJavaScriptDialog(kURL, kConfirmationMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kConfirmationResultCancelled];
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the OK buton is tapped.
- (void)testShowJavaScriptPromptOK {
  // Load the prompt test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kPromptURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, kPromptMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the OK button to close the confirmation.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kPromptMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

  // Wait for the html  to be reset to the input text.
  [ChromeEarlGrey waitForWebStateContainingText:kPromptTestUserInput];
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the Cancel buton is tapped.
- (void)testShowJavaScriptPromptCancelled {
  // Load the prompt test page and tap on the link.
  const GURL kURL = self.testServer->GetURL(kPromptURLPath);
  [ChromeEarlGrey loadURL:kURL];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kTestPageLinkID]];
  [ChromeEarlGrey tapWebStateElementWithID:@(kTestPageLinkID)];
  WaitForJavaScriptDialog(kURL, kPromptMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the Cancel button.
  TapCancel();
  WaitForJavaScriptDialog(kURL, kPromptMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

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
  WaitForJavaScriptDialog(kURL, kAlertLoopMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Tap the OK button to close the alert, then verify that the next alert in
  // the loop is shown.
  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kAlertLoopMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  // Tap the suppress dialogs button.
  TapSuppressDialogsButton();
  WaitForJavaScriptDialog(kURL, kAlertLoopMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

  // Wait for the html  to be reset to the loop finished text.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertLoopFinishedText];
}

// Tests to ensure crbug.com/658260 does not regress.
// Tests that if an alert should be called when settings are displays, the alert
// waits for the dismiss of the settings.
- (void)MAYBE_testShowJavaScriptBehindSettings {
// TODO(crbug.com/40182086): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
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

  // Close the settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Make sure the alert is present.
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertResult];
}

// Tests that an alert is presented after displaying the share menu.
// TODO(crbug.com/41334973): re-enable this test once earl grey can interact
// with the share menu.
- (void)DISABLED_testShowJavaScriptAfterShareMenu {
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
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/true,
                          /*is_main_frame=*/true);

  [[EarlGrey selectElementWithMatcher:OKButton()] performAction:grey_tap()];
  WaitForJavaScriptDialog(kURL, kAlertMessage, /*visible=*/false,
                          /*is_main_frame=*/true);

  // Wait for the expected text to be added to the test page.
  [ChromeEarlGrey waitForWebStateContainingText:kAlertResult];
}

@end
