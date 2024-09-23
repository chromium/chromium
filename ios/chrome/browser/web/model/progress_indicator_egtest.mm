// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/synchronization/condition_variable.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/thread_restrictions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/web/model/progress_indicator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "url/gurl.h"

namespace {

// Text to display in form page.
const char kFormPageText[] = "Form testing page";

// Text to display in infinite loading page.
const char kPageText[] = "Navigation testing page";

// Identifier of form to submit on form page.
const char kFormID[] = "testform";

// URL string for a form page.
const char kFormURL[] = "http://form";

// URL string for an infinite pending page.
const char kInfinitePendingPageURL[] = "http://infinite";

// URL string for a simple page containing `kPageText`.
const char kSimplePageURL[] = "http://simplepage";

// ProgressView from primary toolbar.
id<GREYMatcher> ProgressViewInPrimaryToolbar() {
  return grey_allOf(grey_ancestor(grey_kindOfClassName(@"PrimaryToolbarView")),
                    grey_kindOfClassName(@"MDCProgressView"), nil);
}

// ProgresView from secondary toolbar.
id<GREYMatcher> ProgressViewInSecondaryToolbar() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClassName(@"SecondaryToolbarView")),
      grey_kindOfClassName(@"MDCProgressView"), nil);
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

// Response provider that serves the page which never finishes loading.
// TODO(crbug.com/41311220): Convert this to Embedded Test Server.
class InfinitePendingResponseProvider : public HtmlResponseProvider {
 public:
  explicit InfinitePendingResponseProvider(const GURL& url)
      : url_(url),
        aborted_(false),
        terminated_(false),
        condition_variable_(&lock_) {}
  ~InfinitePendingResponseProvider() override {
    GREYAssert(terminated_, @"Request was not aborted.");
  }

  // Interrupt the current infinite request.
  // Must be called before the object is destroyed.
  void Abort() {
    {
      base::AutoLock auto_lock(lock_);
      aborted_.store(true, std::memory_order_release);
      condition_variable_.Signal();
    }

    const bool success =
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10), ^{
          base::AutoLock auto_lock(lock_);
          return terminated_.load(std::memory_order_acquire);
        });
    GREYAssertTrue(success, @"Timed out trying to Abort()");
  }

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == url_ ||
           request.url == GetInfinitePendingResponseUrl();
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    *headers = GetDefaultResponseHeaders();
    if (request.url == url_) {
      *response_body =
          base::StringPrintf("<p>%s</p><img src='%s'/>", kPageText,
                             GetInfinitePendingResponseUrl().spec().c_str());
    } else {
      *response_body = base::StringPrintf("<p>%s</p>", kPageText);
      {
        base::AutoLock auto_lock(lock_);
        while (!aborted_.load(std::memory_order_acquire))
          condition_variable_.Wait();
        terminated_.store(true, std::memory_order_release);
      }
    }
  }

 private:
  // Returns a url for which this response provider will never reply.
  GURL GetInfinitePendingResponseUrl() const {
    GURL::Replacements replacements;
    replacements.SetPathStr("resource");
    return url_.DeprecatedGetOriginAsURL().ReplaceComponents(replacements);
  }

  // Main page URL that never finish loading.
  GURL url_;

  // Everything below is protected by lock_.
  mutable base::Lock lock_;
  std::atomic_bool aborted_;
  std::atomic_bool terminated_;
  base::ConditionVariable condition_variable_;
};

}  // namespace

// Tests webpage loading progress indicator.
@interface ProgressIndicatorTestCase : WebHttpServerChromeTestCase
@end

@implementation ProgressIndicatorTestCase

// Returns an HTML string for a form with the submission action set to
// `submitURL`.
- (std::string)formPageHTMLWithFormSubmitURL:(GURL)submitURL {
  return base::StringPrintf(
      "<p>%s</p><form id='%s' method='post' action='%s'>"
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
      web::test::HttpServer::MakeUrl(kInfinitePendingPageURL);
  auto uniqueInfinitePendingProvider =
      std::make_unique<InfinitePendingResponseProvider>(infinitePendingURL);
  InfinitePendingResponseProvider* infinitePendingProvider =
      uniqueInfinitePendingProvider.get();
  web::test::SetUpHttpServer(std::move(uniqueInfinitePendingProvider));

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
  infinitePendingProvider->Abort();
}

// Tests that the progress indicator is shown and has expected progress value
// after a form is submitted, and the toolbar is visible.
- (void)testProgressIndicatorShownOnFormSubmit {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  const GURL infinitePendingURL =
      web::test::HttpServer::MakeUrl(kInfinitePendingPageURL);

  // Create a page with a form to test.
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithFormSubmitURL:infinitePendingURL];
  web::test::SetUpSimpleHttpServer(responses);

  // Add responseProvider for page that never finishes loading.
  auto uniqueInfinitePendingProvider =
      std::make_unique<InfinitePendingResponseProvider>(infinitePendingURL);
  InfinitePendingResponseProvider* infinitePendingProvider =
      uniqueInfinitePendingProvider.get();
  web::test::AddResponseProvider(std::move(uniqueInfinitePendingProvider));

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
  infinitePendingProvider->Abort();
}

// Tests that the progress indicator disappears after form has been submitted.
- (void)testProgressIndicatorDisappearsAfterFormSubmit {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  const GURL simplePageURL = web::test::HttpServer::MakeUrl(kSimplePageURL);

  // Create a page with a form to test.
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithFormSubmitURL:simplePageURL];
  responses[simplePageURL] = kPageText;
  web::test::SetUpSimpleHttpServer(responses);

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
  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithSuppressedSubmitEvent];
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:formURL];

  // Verify the form page has been loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kFormPageText];

  [ChromeEarlGrey submitWebStateFormWithID:kFormID];

  // Verify progress view is not visible.
  CheckProgressViewNotVisible();
}

@end
