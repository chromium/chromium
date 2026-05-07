// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/synchronization/condition_variable.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/thread_restrictions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/web/model/progress_indicator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

// Text to display in form page.
const char kFormPageText[] = "Form testing page";

// Text to display in infinite loading page.
const char kPageText[] = "Navigation testing page";

// Identifier of form to submit on form page.
const char kFormID[] = "testform";

// URL strings for form pages with different behaviors.
const char kFormInfiniteURL[] = "/form_infinite";
const char kFormSimpleURL[] = "/form_simple";
const char kFormSuppressedURL[] = "/form_suppressed";

// URL string for an infinite pending page.
const char kInfinitePendingPageURL[] = "/infinite";

// URL string for a simple page containing `kPageText`.
const char kSimplePageURL[] = "/simplepage";

// ProgressView from primary toolbar.
id<GREYMatcher> ProgressViewInPrimaryToolbar() {
  return grey_allOf(grey_ancestor(grey_kindOfClassName(@"PrimaryToolbarView")),
                    grey_kindOfClassName(@"UIProgressView"), nil);
}

// ProgresView from secondary toolbar.
id<GREYMatcher> ProgressViewInSecondaryToolbar() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClassName(@"SecondaryToolbarView")),
      grey_kindOfClassName(@"UIProgressView"), nil);
}

// Matcher for `progressView` that should be visible at `progress`.
id<GREYMatcher> ProgressViewAtProgress(id<GREYMatcher> progressView,
                                       CGFloat progress) {
  return grey_allOf(
      progressView,
      [ProgressIndicatorAppInterface progressViewWithProgress:progress], nil);
}

// Checks that the progress view is visible with `progress` in the toolbar
// containing the omnibox.
void CheckProgressViewVisibleWithProgress(CGFloat progress) {
  id<GREYMatcher> visibleProgressView = ProgressViewInPrimaryToolbar();
  id<GREYMatcher> hiddenProgressView = ProgressViewInSecondaryToolbar();
  if ([ChromeEarlGrey isUnfocusedOmniboxAtBottom]) {
    visibleProgressView = ProgressViewInSecondaryToolbar();
    hiddenProgressView = ProgressViewInPrimaryToolbar();
  }

  [[EarlGrey selectElementWithMatcher:ProgressViewAtProgress(
                                          visibleProgressView, progress)]
      assertWithMatcher:grey_sufficientlyVisible()];

  if ([ChromeEarlGrey isBottomOmniboxAvailable]) {
    [[EarlGrey selectElementWithMatcher:hiddenProgressView]
        assertWithMatcher:grey_notVisible()];
  }
}

// Checks that progress view from both toolbars are not visible.
void CheckProgressViewNotVisible() {
  [[EarlGrey selectElementWithMatcher:ProgressViewInPrimaryToolbar()]
      assertWithMatcher:grey_notVisible()];

  if ([ChromeEarlGrey isBottomOmniboxAvailable]) {
    [[EarlGrey selectElementWithMatcher:ProgressViewInSecondaryToolbar()]
        assertWithMatcher:grey_notVisible()];
  }
}

std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    const std::string& content) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(content);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> NotFoundResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

}  // namespace

// Tests webpage loading progress indicator.
@interface ProgressIndicatorTestCase : ChromeTestCase
@end

@implementation ProgressIndicatorTestCase

- (void)setUp {
  [super setUp];

  __weak ProgressIndicatorTestCase* weakSelf = self;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(^std::unique_ptr<net::test_server::HttpResponse>(
          const net::test_server::HttpRequest& request) {
        return [weakSelf handleProgressIndicatorRequest:request];
      }));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Handles requests for the embedded test server.
- (std::unique_ptr<net::test_server::HttpResponse>)
    handleProgressIndicatorRequest:
        (const net::test_server::HttpRequest&)request {
  if (request.relative_url == kInfinitePendingPageURL) {
    return CreateHttpResponse(
        base::StringPrintf("<p>%s</p><img src='/resource'/>", kPageText));
  }

  if (request.relative_url == "/resource") {
    return std::make_unique<net::test_server::DelayedHttpResponse>(
        base::Minutes(10));
  }

  if (request.relative_url == kFormInfiniteURL) {
    GURL infinitePendingURL = self.testServer->GetURL(kInfinitePendingPageURL);
    return CreateHttpResponse(
        [self formPageHTMLWithFormSubmitURL:infinitePendingURL]);
  }

  if (request.relative_url == kFormSimpleURL) {
    GURL simplePageURL = self.testServer->GetURL(kSimplePageURL);
    return CreateHttpResponse(
        [self formPageHTMLWithFormSubmitURL:simplePageURL]);
  }

  if (request.relative_url == kFormSuppressedURL) {
    return CreateHttpResponse([self formPageHTMLWithSuppressedSubmitEvent]);
  }

  if (request.relative_url == kSimplePageURL) {
    return CreateHttpResponse(kPageText);
  }

  return NotFoundResponse();
}

// Returns an HTML string for a form with the submission action set to
// `submitURL`.
- (std::string)formPageHTMLWithFormSubmitURL:(GURL)submitURL {
  return base::StringPrintf("<p>%s</p><form id='%s' method='post' action='%s'>"
                            "<input type='submit'></form>",
                            kFormPageText, kFormID, submitURL.spec().c_str());
}

// Returns an HTML string for a form with a submit event that returns false.
- (std::string)formPageHTMLWithSuppressedSubmitEvent {
  return base::StringPrintf(
      "<p>%s</p><form id='%s' method='post' onsubmit='return false'>"
      "<input type='submit'></form>",
      kFormPageText, kFormID);
}

// Tests that the progress indicator is shown and has expected progress value
// for a simple two item page, and the toolbar is visible.
- (void)testProgressIndicatorShown {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // Load a page which never finishes loading.
  const GURL infinitePendingURL =
      self.testServer->GetURL(kInfinitePendingPageURL);

  // EG synchronizes with WKWebView. Disable synchronization for EG interation
  // during when page is loading.
  ScopedSynchronizationDisabler disabler;

  // The page being loaded never completes, so call the LoadUrl helper that
  // does not wait for the page to complete loading.
  [ChromeEarlGrey loadURL:infinitePendingURL waitForCompletion:NO];

  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify progress view visible and halfway progress.
  CheckProgressViewVisibleWithProgress(0.5);

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the progress indicator is shown and has expected progress value
// after a form is submitted, and the toolbar is visible.
- (void)testProgressIndicatorShownOnFormSubmit {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  const GURL formURL = self.testServer->GetURL(kFormInfiniteURL);

  // Add responseProvider for page that never finishes loading.

  // EG synchronizes with WKWebView. Disable synchronization for EG interation
  // during when page is loading.
  ScopedSynchronizationDisabler disabler;

  // Load form first.
  [ChromeEarlGrey loadURL:formURL];
  [ChromeEarlGrey waitForWebStateContainingText:kFormPageText];

  [ChromeEarlGrey submitWebStateFormWithID:kFormID];

  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify progress view visible and halfway progress.
  CheckProgressViewVisibleWithProgress(0.5);

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the progress indicator disappears after form has been submitted.
- (void)testProgressIndicatorDisappearsAfterFormSubmit {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  const GURL formURL = self.testServer->GetURL(kFormSimpleURL);

  [ChromeEarlGrey loadURL:formURL];

  [ChromeEarlGrey waitForWebStateContainingText:kFormPageText];

  [ChromeEarlGrey submitWebStateFormWithID:kFormID];

  // Verify the new page has been loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];

  // Verify progress view is not visible.
  CheckProgressViewNotVisible();
}

// Tests that the progress indicator disappears after form post attempt with a
// submit event that returns false.
- (void)testProgressIndicatorDisappearsAfterSuppressedFormPost {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // Create a page with a form to test.
  const GURL formURL = self.testServer->GetURL(kFormSuppressedURL);

  [ChromeEarlGrey loadURL:formURL];

  // Verify the form page has been loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kFormPageText];

  [ChromeEarlGrey submitWebStateFormWithID:kFormID];

  // Verify progress view is not visible.
  CheckProgressViewNotVisible();
}

@end
