// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::TapWebElement;
using chrome_test_util::WebViewMatcher;

using testing::ElementToDismissAlert;

namespace {

// Response shown on the page of `GetDestinationUrl`.
constexpr char kDestinationText[] = "bar!";

// Response shown on the page of `GetGenericUrl`.
constexpr char kGenericText[] = "A generic page";

// Label for the button in the form.
NSString* const kSubmitButtonLabel = @"submit";

// Html form template with a submission button named "submit".
constexpr char kFormHtmlTemplate[] =
    "<form method='post' action='%s'> submit: "
    "<input value='textfield' id='textfield' type='text'></label>"
    "<input type='submit' value='submit' id='submit'>"
    "</form>";

std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    const std::string& content) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(content);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    net::test_server::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_FOUND);
  response->AddCustomHeader("Location", server->GetURL("/destination").spec());
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleGenericRequest(
    const net::test_server::HttpRequest& request) {
  return CreateHttpResponse("A generic page");
}

std::unique_ptr<net::test_server::HttpResponse> HandleFormRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method == net::test_server::METHOD_POST) {
    return CreateHttpResponse("POST " + request.content);
  }
  return CreateHttpResponse(
      "<form method='post'>"
      "<input value='button' type='submit' id='button'></form>");
}

std::unique_ptr<net::test_server::HttpResponse> HandleFormRedirectRequest(
    net::test_server::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  return CreateHttpResponse(base::StringPrintf(
      "<form method='post' action='%s'> submit: "
      "<input value='textfield' id='textfield' type='text'></label>"
      "<input type='submit' value='submit' id='submit'>"
      "</form>",
      server->GetURL("/redirect").spec().c_str()));
}

std::unique_ptr<net::test_server::HttpResponse> HandleDestinationRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method == net::test_server::METHOD_POST) {
    return CreateHttpResponse("POST " + request.content);
  }
  return CreateHttpResponse("GET ");
}

std::unique_ptr<net::test_server::HttpResponse> HandleFormsRequest(
    std::map<std::string, std::string>* responses,
    net::test_server::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  const std::string path{request.GetURL().path()};
  auto it = responses->find(path);
  if (it != responses->end()) {
    return CreateHttpResponse(it->second);
  }

  if (path == "/redirect") {
    return HandleRedirectRequest(server, request);
  }
  if (path == "/generic") {
    return HandleGenericRequest(request);
  }
  if (path == "/form") {
    return HandleFormRequest(request);
  }
  if (path == "/formRedirect") {
    return HandleFormRedirectRequest(server, request);
  }
  if (path == "/destination") {
    return HandleDestinationRequest(request);
  }

  return nullptr;
}
}  // namespace

// Tests submition of HTTP forms POST data including cases involving navigation.
@interface FormsTestCase : ChromeTestCase {
  std::map<std::string, std::string> _responses;
}
@end

@implementation FormsTestCase

- (GURL)genericUrl {
  return self.testServer->GetURL("/generic");
}

- (GURL)formUrl {
  return self.testServer->GetURL("/form");
}

- (GURL)formPostOnSamePageUrl {
  return self.testServer->GetURL("/form");
}

- (GURL)destinationUrl {
  return self.testServer->GetURL("/destination");
}

- (GURL)redirectUrl {
  return self.testServer->GetURL("/redirect");
}

- (GURL)redirectFormUrl {
  return self.testServer->GetURL("/formRedirect");
}

- (void)setUp {
  [super setUp];
  _responses.clear();

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &HandleFormsRequest, base::Unretained(&_responses), self.testServer));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Matcher for a Go button that is interactable.
id<GREYMatcher> GoButtonMatcher() {
  return grey_allOf(grey_accessibilityID(@"Go"), grey_interactable(), nil);
}

// Matcher for the resend POST button in the repost warning dialog.
id<GREYMatcher> ResendPostButtonMatcher() {
  return chrome_test_util::AlertItemWithAccessibilityLabelId(
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
  responses[[self genericUrl]] = kGenericText;
  responses[[self formUrl]] = base::StringPrintf(
      kFormHtmlTemplate, [self destinationUrl].spec().c_str());
  responses[[self destinationUrl]] = kDestinationText;

  for (const auto& pair : responses) {
    _responses[std::string(pair.first.path())] = pair.second;
  }
}

// Tests that a POST followed by reloading the destination page resends data.
- (void)testRepostFormAfterReload {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

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
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
}

// Tests that a POST followed by navigating to a new page and then tapping back
// to the form result page resends data.
- (void)testRepostFormAfterTappingBack {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  // Go to a new page and go back and check that the data is reposted.
  [ChromeEarlGrey loadURL:[self genericUrl]];
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
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
}

// Tests that a POST followed by tapping back to the form page and then tapping
// forward to the result page resends data.
- (void)testRepostFormAfterTappingBackAndForward {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

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
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
}

// Tests that a POST followed by a new request and then index navigation to get
// back to the result page resends data.
- (void)testRepostFormAfterIndexNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  // Go to a new page and go back to destination through back history.
  [ChromeEarlGrey loadURL:[self genericUrl]];
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
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
}

// When data is not reposted, the request is canceled.
- (void)testRepostFormCancelling {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

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

    if (@available(iOS 26, *)) {
      [ChromeEarlGreyUI
          dismissByTappingOnTheWindowOfPopover:ResendPostButtonMatcher()];
    } else {
      [[EarlGrey selectElementWithMatcher:ElementToDismissAlert(@"Cancel")]
          performAction:grey_tap()];
    }
  }

  [ChromeEarlGrey waitForPageToFinishLoading];

  // NavigationManagerImpl displays repost on `reload`. So after
  // cancelling, web view should show `destinationURL`.
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
}

// A new navigation dismisses the repost dialog.
- (void)testRepostFormDismissedByNewNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

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
  [ChromeEarlGrey loadURL:[self genericUrl]];
  [ChromeEarlGrey waitForWebStateContainingText:kGenericText];

  // Repost dialog should not be visible anymore.
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that pressing the button on a POST-based form changes the page and that
// the back button works as expected afterwards.
- (void)testGoBackButtonAfterFormSubmission {
  [self setUpFormTestSimpleHttpServer];
  GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationText];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  // Go back and verify the browser navigates to the original URL.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:(base::SysNSStringToUTF8(
                                                    kSubmitButtonLabel))];
  [ChromeEarlGrey waitForWebStateVisibleURL:[self formUrl]];
}

// Tests that a POST followed by a redirect does not show the popup.
// TODO(crbug.com/495387290): Flaky on iPad simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testRepostFormCancellingAfterRedirect \
  FLAKY_testRepostFormCancellingAfterRedirect
#else
#define MAYBE_testRepostFormCancellingAfterRedirect \
  testRepostFormCancellingAfterRedirect
#endif
- (void)MAYBE_testRepostFormCancellingAfterRedirect {
#if TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Flaky on iPad simulator.");
  }
#endif

  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self redirectFormUrl]];

  // Submit the form, which redirects before printing the data.
  [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel];

  // Check that the redirect changes the POST to a GET.
  [ChromeEarlGrey waitForWebStateContainingText:"GET"];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  [ChromeEarlGrey reload];

  // Check that the popup did not show
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey waitForWebStateContainingText:"GET"];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];
}

// Tests that pressing the button on a POST-based form with same-page action
// does not change the page URL and that the back button works as expected
// afterwards.
- (void)testPostFormToSamePage {
  const GURL formURL = [self formPostOnSamePageUrl];

  // Open the first URL so it's in history.
  [ChromeEarlGrey loadURL:[self genericUrl]];

  // Open the second URL, tap the button, and verify the browser navigates to
  // the expected URL.
  [ChromeEarlGrey loadURL:formURL];
  [ChromeEarlGrey tapWebStateElementWithID:@"button"];
  [ChromeEarlGrey waitForWebStateContainingText:"POST"];
  [ChromeEarlGrey waitForWebStateVisibleURL:formURL];

  // Go back once and verify the browser navigates to the form URL.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateVisibleURL:formURL];

  // Go back a second time and verify the browser navigates to the first URL.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateVisibleURL:[self genericUrl]];
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
  const GURL destinationURL = [self destinationUrl];

  [ChromeEarlGrey loadURL:[self formUrl]];
  [self submitFormUsingKeyboardGoButtonWithInputID:"textfield"];

  // Verify that the browser navigates to the expected URL.
  [ChromeEarlGrey waitForWebStateContainingText:"bar!"];
  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  // Go back and verify that the browser navigates to the original URL.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateVisibleURL:[self formUrl]];
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

    // There's currently no EG API to tap 'go' on the keyboard.
    XCUIApplication* app = [[XCUIApplication alloc] init];
    [[[app keyboards] buttons][@"go"] tap];
  }
}

@end
