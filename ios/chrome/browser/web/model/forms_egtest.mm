// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;
using chrome_test_util::TapWebElement;
using chrome_test_util::WebViewMatcher;

using testing::ElementToDismissAlert;

namespace {

// Response shown on the page of `GetDestinationUrl`.
constexpr char kDestinationText[] = "bar!";

// Response shown on the page of `GetGenericUrl`.
constexpr char kGenericText[] = "A generic page";

// Label for the button in the form.
NSString* kSubmitButtonLabel = @"submit";

// Html form template with a submission button named "submit".
constexpr char kFormHtmlTemplate[] =
    "<form method='post' action='%s'> submit: "
    "<input value='textfield' id='textfield' type='text'></label>"
    "<input type='submit' value='submit' id='submit'>"
    "</form>";

// GURL of a generic website in the user navigation flow.
const GURL GetGenericUrl() {
  return web::test::HttpServer::MakeUrl("http://generic");
}

// GURL of a page with a form that posts data to `GetDestinationUrl`.
const GURL GetFormUrl() {
  return web::test::HttpServer::MakeUrl("http://form");
}

// GURL of a page with a form that posts data to `GetDestinationUrl`.
const GURL GetFormPostOnSamePageUrl() {
  return web::test::HttpServer::MakeUrl("http://form");
}

// GURL of the page to which the `GetFormUrl` posts data to.
const GURL GetDestinationUrl() {
  return web::test::HttpServer::MakeUrl("http://destination");
}

#pragma mark - TestFormResponseProvider

// URL that redirects to `GetDestinationUrl` with a 302.
const GURL GetRedirectUrl() {
  return web::test::HttpServer::MakeUrl("http://redirect");
}

// URL to return a page that posts to `GetRedirectUrl`.
const GURL GetRedirectFormUrl() {
  return web::test::HttpServer::MakeUrl("http://formRedirect");
}

// A ResponseProvider that provides html response, post response or a redirect.
class TestFormResponseProvider : public web::DataResponseProvider {
 public:
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;
};

bool TestFormResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return url == GetDestinationUrl() || url == GetRedirectUrl() ||
         url == GetRedirectFormUrl() || url == GetFormPostOnSamePageUrl() ||
         url == GetGenericUrl();
}

void TestFormResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  if (url == GetRedirectUrl()) {
    *headers = web::ResponseProvider::GetRedirectResponseHeaders(
        GetDestinationUrl().spec(), net::HTTP_FOUND);
    return;
  }

  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url == GetGenericUrl()) {
    *response_body = kGenericText;
    return;
  }
  if (url == GetFormPostOnSamePageUrl()) {
    if (request.method == "POST") {
      *response_body = request.method + std::string(" ") + request.body;
    } else {
      *response_body =
          "<form method='post'>"
          "<input value='button' type='submit' id='button'></form>";
    }
    return;
  }

  if (url == GetRedirectFormUrl()) {
    *response_body =
        base::StringPrintf(kFormHtmlTemplate, GetRedirectUrl().spec().c_str());
    return;
  }
  if (url == GetDestinationUrl()) {
    *response_body = request.method + std::string(" ") + request.body;
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

// Tests submition of HTTP forms POST data including cases involving navigation.
@interface FormsTestCase : WebHttpServerChromeTestCase
@end

@implementation FormsTestCase

// Matcher for a Go button that is interactable.
id<GREYMatcher> GoButtonMatcher() {
  return grey_allOf(grey_accessibilityID(@"Go"), grey_interactable(), nil);
}

// Matcher for the resend POST button in the repost warning dialog.
id<GREYMatcher> ResendPostButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_HTTP_POST_WARNING_RESEND);
}

// Open back navigation history.
- (void)openBackHistory {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
}

// Accepts the warning that the form POST data will be reposted.
- (void)confirmResendWarning {
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      performAction:grey_longPress()];
}

// Sets up a basic simple http server for form test with a form located at
// `GetFormUrl`, and posts data to `GetDestinationUrl` upon submission.
- (void)setUpFormTestSimpleHttpServer {
  std::map<GURL, std::string> responses;
  responses[GetGenericUrl()] = kGenericText;
  responses[GetFormUrl()] =
      base::StringPrintf(kFormHtmlTemplate, GetDestinationUrl().spec().c_str());
  responses[GetDestinationUrl()] = kDestinationText;
  web::test::SetUpSimpleHttpServer(responses);
}

// Tests that a POST followed by reloading the destination page resends data.
- (void)testRepostFormAfterReload {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Repost confirmation dialog is presented before loading stops so do not wait
  // for load to complete because it never will.
  [ChromeEarlGrey reloadAndWaitForCompletion:NO];

  {
    // Synchronization must be disabled until after the repost confirmation is
    // dismissed because it is presented during the load. It is always disabled,
    // but immediately re-enabled if slim navigation manger is not enabled. This
    // is necessary in order to keep the correct scope of
    // ScopedSynchronizationDisabler which ensures synchronization is not left
    // disabled if the test fails.
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();
    // TODO(crbug.com/41473918): Investigate why this is necessary even with a
    // visible check below.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));

    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()];
    [self confirmResendWarning];
  }

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by navigating to a new page and then tapping back
// to the form result page resends data.
- (void)testRepostFormAfterTappingBack {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go to a new page and go back and check that the data is reposted.
  [ChromeEarlGrey loadURL:GetGenericUrl()];
  [ChromeEarlGrey goBack];

  // NavigationManager doesn't trigger repost on `goForward` due to WKWebView's
  // back-forward cache. Force reload to trigger repost. Not waiting because
  // NavigationManager presents repost confirmation dialog before loading stops.
  [ChromeEarlGrey reloadAndWaitForCompletion:NO];

  {
    // Synchronization must be disabled until after the repost confirmation is
    // dismissed because it is presented during the load. It is always disabled,
    // but immediately re-enabled if slim navigation manger is not enabled. This
    // is necessary in order to keep the correct scope of
    // ScopedSynchronizationDisabler which ensures synchronization is not left
    // disabled if the test fails.
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();
    // TODO(crbug.com/41473918): Investigate why this is necessary even with a
    // visible check below.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));

    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()];
    [self confirmResendWarning];
  }

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by tapping back to the form page and then tapping
// forward to the result page resends data.
- (void)testRepostFormAfterTappingBackAndForward {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];

  // NavigationManager doesn't trigger repost on `goForward` due to WKWebView's
  // back-forward cache. Force reload to trigger repost. Not waiting because
  // NavigationManager presents repost confirmation dialog before loading stops.
  [ChromeEarlGrey reloadAndWaitForCompletion:NO];

  {
    // Synchronization must be disabled until after the repost confirmation is
    // dismissed because it is presented during the load. It is always disabled,
    // but immediately re-enabled if slim navigation manger is not enabled. This
    // is necessary in order to keep the correct scope of
    // ScopedSynchronizationDisabler which ensures synchronization is not left
    // disabled if the test fails.
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();
    // TODO(crbug.com/41473918): Investigate why this is necessary even with a
    // visible check below.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));

    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()];
    [self confirmResendWarning];
  }

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by a new request and then index navigation to get
// back to the result page resends data.
- (void)testRepostFormAfterIndexNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go to a new page and go back to destination through back history.
  [ChromeEarlGrey loadURL:GetGenericUrl()];
  [self openBackHistory];

  // Mimic `web::GetDisplayTitleForUrl` behavior which uses FormatUrl
  // internally. It can't be called directly from the EarlGrey 2 test process.
  NSString* title =
      base::SysUTF16ToNSString(url_formatter::FormatUrl(destinationURL));
  id<GREYMatcher> historyItem =
      chrome_test_util::ContextMenuItemWithAccessibilityLabel(title);
  [[EarlGrey selectElementWithMatcher:historyItem]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:historyItem] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Back-forward navigation is served from WKWebView's app-cache, so it won't
  // trigger repost warning.
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// When data is not reposted, the request is canceled.
- (void)testRepostFormCancelling {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];

  // NavigationManager doesn't trigger repost on `goForward` due to WKWebView's
  // back-forward cache. Force reload to trigger repost. Not waiting because
  // NavigationManager presents repost confirmation dialog before loading stops.
  [ChromeEarlGrey reloadAndWaitForCompletion:NO];

  {
    // Synchronization must be disabled until after the repost confirmation is
    // dismissed because it is presented during the load. It is always disabled,
    // but immediately re-enabled if slim navigation manger is not enabled. This
    // is necessary in order to keep the correct scope of
    // ScopedSynchronizationDisabler which ensures synchronization is not left
    // disabled if the test fails.
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();

    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()];
    [[EarlGrey selectElementWithMatcher:ElementToDismissAlert(@"Cancel")]
        performAction:grey_tap()];
  }

  [ChromeEarlGrey waitForPageToFinishLoading];

  // NavigationManagerImpl displays repost on `reload`. So after
  // cancelling, web view should show `destinationURL`.
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
}

// A new navigation dismisses the repost dialog.
- (void)testRepostFormDismissedByNewNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Repost confirmation dialog is presented before loading stops so do not wait
  // for load to complete because it never will.
  [ChromeEarlGrey reloadAndWaitForCompletion:NO];

  {
    // Synchronization must be disabled until after the repost confirmation is
    // dismissed because it is presented during the load. It is always disabled,
    // but immediately re-enabled if slim navigation manger is not enabled. This
    // is necessary in order to keep the correct scope of
    // ScopedSynchronizationDisabler which ensures synchronization is not left
    // disabled if the test fails.
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();

    // Repost confirmation box should be visible.
    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()];
  }

  // Starting a new navigation while the repost dialog is presented should not
  // crash.
  [ChromeEarlGrey loadURL:GetGenericUrl()];
  [ChromeEarlGrey waitForWebStateContainingText:kGenericText];

  // Repost dialog should not be visible anymore.
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that pressing the button on a POST-based form changes the page and that
// the back button works as expected afterwards.
- (void)testGoBackButtonAfterFormSubmission {
  [self setUpFormTestSimpleHttpServer];
  GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify the browser navigates to the original URL.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:(base::SysNSStringToUTF8(
                                                    kSubmitButtonLabel))];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFormUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by a redirect does not show the popup.
- (void)testRepostFormCancellingAfterRedirect {
  web::test::SetUpHttpServer(std::make_unique<TestFormResponseProvider>());
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetRedirectFormUrl()];

  // Submit the form, which redirects before printing the data.
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];

  // Check that the redirect changes the POST to a GET.
  [ChromeEarlGrey waitForWebStateContainingText:"GET"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey reload];

  // Check that the popup did not show
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey waitForWebStateContainingText:"GET"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that pressing the button on a POST-based form with same-page action
// does not change the page URL and that the back button works as expected
// afterwards.
- (void)testPostFormToSamePage {
  web::test::SetUpHttpServer(std::make_unique<TestFormResponseProvider>());
  const GURL formURL = GetFormPostOnSamePageUrl();

  // Open the first URL so it's in history.
  [ChromeEarlGrey loadURL:GetGenericUrl()];

  // Open the second URL, tap the button, and verify the browser navigates to
  // the expected URL.
  [ChromeEarlGrey loadURL:formURL];
  [ChromeEarlGrey tapWebStateElementWithID:@"button"];
  [ChromeEarlGrey waitForWebStateContainingText:"POST"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back once and verify the browser navigates to the form URL.
  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back a second time and verify the browser navigates to the first URL.
  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetGenericUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that submitting a POST-based form by tapping the 'Go' button on the
// keyboard navigates to the correct URL and the back button works as expected
// afterwards.
// TODO:(crbug.com/1147654): re-enable after figuring out why it is failing.
- (void)DISABLE_testPostFormEntryWithKeyboard {
  // Test fails on iPad Air 2 13.4 crbug.com/1102608.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails in iOS 13 on iPads.");
  }

  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  [ChromeEarlGrey loadURL:GetFormUrl()];
  [self submitFormUsingKeyboardGoButtonWithInputID:"textfield"];

  // Verify that the browser navigates to the expected URL.
  [ChromeEarlGrey waitForWebStateContainingText:"bar!"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify that the browser navigates to the original URL.
  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFormUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tap the text field indicated by `ID` to open the keyboard, and then
// press the keyboard's "Go" button to submit the form.
- (void)submitFormUsingKeyboardGoButtonWithInputID:(const std::string&)ID {
  // Disable EarlGrey's synchronization since it is blocked by opening the
  // keyboard from a web view.
  {
    ScopedSynchronizationDisabler disabler;

    // Wait for web view to be interactable before tapping.
    GREYCondition* interactableCondition = [GREYCondition
        conditionWithName:@"Wait for web view to be interactable."
                    block:^BOOL {
                      NSError* error = nil;
                      [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
                          assertWithMatcher:grey_interactable()
                                      error:&error];
                      return !error;
                    }];
    GREYAssert([interactableCondition
                   waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                       .InSecondsF()],
               @"Web view did not become interactable.");

    [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
        performAction:TapWebElement(
                          [ElementSelector selectorWithElementID:ID])];

    // Wait for the accessory icon to appear.
    [ChromeEarlGrey waitForKeyboardToAppear];

    // TODO(crbug.com/40227513): Move this logic into EG.
    XCUIApplication* app = [[XCUIApplication alloc] init];
    [[[app keyboards] buttons][@"go"] tap];
  }
}

@end
