// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#import <memory>

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "url/gurl.h"

using chrome_test_util::OmniboxText;

namespace {

const char kTestPage1[] = "Test Page 1";
const char kTestPage2[] = "Test Page 2";
const char kTestPage3[] = "Test Page 3";
const char kGoBackLink[] = "go-back";
const char kGoForwardLink[] = "go-forward";
const char kGoNegativeDeltaLink[] = "go-negative-delta";
const char kGoNegativeDeltaTwiceLink[] = "go-negative-delta-twice";
const char kGoPositiveDeltaLink[] = "go-positive-delta";
const char kPage1Link[] = "page-1";
const char kPage2Link[] = "page-2";
const char kPage3Link[] = "page-3";

// Response provider which can be paused. When it is paused it buffers all
// requests and does not respond to them until `set_paused(false)` is called.
class PausableResponseProvider : public HtmlResponseProvider {
 public:
  explicit PausableResponseProvider(
      const std::map<GURL, std::string>& responses)
      : HtmlResponseProvider(responses) {}

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* body) override {
    set_last_request_url(request.url);
    while (is_paused()) {
    }
    HtmlResponseProvider::GetResponseHeadersAndBody(request, headers, body);
  }
  // URL for the last seen request.
  const GURL& last_request_url() const {
    base::AutoLock autolock(lock_);
    return last_request_url_;
  }

  // If true buffers all incoming requests and will not reply until is_paused is
  // changed to false.
  bool is_paused() const {
    base::AutoLock autolock(lock_);
    return is_paused_;
  }
  void set_paused(bool paused) {
    base::AutoLock last_request_url_autolock(lock_);
    is_paused_ = paused;
  }

 private:
  void set_last_request_url(const GURL& url) {
    base::AutoLock last_request_url_autolock(lock_);
    last_request_url_ = url;
  }

  mutable base::Lock lock_;
  GURL last_request_url_;
  bool is_paused_ = false;
};

}  // namespace

// Test case for back forward and delta navigations focused on making sure that
// omnibox visible URL always represents the current page.
@interface VisibleURLWithCachedRestoreTestCase : WebHttpServerChromeTestCase {
  raw_ptr<PausableResponseProvider> _responseProvider;
  GURL _testURL1;
  GURL _testURL2;
  GURL _testURL3;
}

// Spec of the last request URL that reached the server.
@property(nonatomic, copy, readonly) NSString* lastRequestURLSpec;

// Pauses response server.
- (void)setServerPaused:(BOOL)paused;

// Waits until `_responseProvider` receives a request with the given `URL`.
// Returns YES if request was received, NO on timeout.
- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL;

@end

@implementation VisibleURLWithCachedRestoreTestCase

- (void)setUp {
  [super setUp];

  _testURL1 = web::test::HttpServer::MakeUrl("http://url1.test/");
  _testURL2 = web::test::HttpServer::MakeUrl("http://url2.test/");
  _testURL3 = web::test::HttpServer::MakeUrl("http://url3.test/");

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  responses[_testURL1] = [self pageContentForTitle:kTestPage1];
  responses[_testURL2] = [self pageContentForTitle:kTestPage2];
  responses[_testURL3] = [self pageContentForTitle:kTestPage3];

  std::unique_ptr<PausableResponseProvider> unique_provider =
      std::make_unique<PausableResponseProvider>(responses);
  _responseProvider = unique_provider.get();
  web::test::SetUpHttpServer(std::move(unique_provider));

  [ChromeEarlGrey loadURL:_testURL1];
  [ChromeEarlGrey loadURL:_testURL2];
}

- (std::string)pageContentForTitle:(const char*)title {
  // Every page has links for window.history navigations (back, forward, go).
  return base::StringPrintf(
      "<head><title>%s</title></head>"
      "<body>%s<br/>"
      "<a onclick='window.history.back()' id='%s'>Go Back</a><br/>"
      "<a onclick='window.history.forward()' id='%s'>Go Forward</a><br/>"
      "<a onclick='window.history.go(-1)' id='%s'>Go Delta -1</a><br/>"
      "<a onclick='window.history.go(-2)' id='%s'>Go Delta -2</a><br/>"
      "<a onclick='window.history.go(1)' id='%s'>Go Delta +1</a><br/>"
      "<a href='%s' id='%s'>Page 1</a><br/>"
      "<a href='%s' id='%s'>Page 2</a><br/>"
      "<a href='%s' id='%s'>Page 3</a><br/>"
      "</body>",
      title, title, kGoBackLink, kGoForwardLink, kGoNegativeDeltaLink,
      kGoNegativeDeltaTwiceLink, kGoPositiveDeltaLink, _testURL1.spec().c_str(),
      kPage1Link, _testURL2.spec().c_str(), kPage2Link,
      _testURL3.spec().c_str(), kPage3Link);
}

#pragma mark -
#pragma mark Tests

// Tests that visible URL is always the pending URL during
// pending back and forward navigations.
- (void)testBackForwardNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the back button in the toolbar and verify that URL1 is displayed.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        performAction:grey_tap()];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
               @"Last request URL: %@", self.lastRequestURLSpec);
    // Make server respond so URL1 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the back button in the toolbar and verify that URL1 is displayed.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
        performAction:grey_tap()];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
               @"Last request URL: %@", self.lastRequestURLSpec);
    // Make server respond so URL2 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the pending URL during
// navigations initiated from back history popover.
- (void)testHistoryNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];

  // Pauses response server and disables EG synchronization.
  // Pending navigation will not complete until server is unpaused.
  {
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];
  }

  // Go back in history and verify that URL1 is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
  NSString* URL1Title = base::SysUTF8ToNSString(kTestPage1);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     URL1Title)] performAction:grey_tap()];

  {
    // Disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
               @"Last request URL: %@", self.lastRequestURLSpec);

    // Make server respond so URL1 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that stopping a pending Back navigation and reloading reloads the
// pending URL.
- (void)testStoppingPendingBackNavigationAndReload {
  // With iPhone, Stop and Reload are in the tool menu. There's no easy way to
  // track some animations (opening a popop) and not others (load progress bar)
  // which makes this test fail.
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPhone (sync issues)");
  }
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  {
    std::unique_ptr<ScopedSynchronizationDisabler> disabler =
        std::make_unique<ScopedSynchronizationDisabler>();
    [self setServerPaused:YES];

    // Tap the back button, stop pending navigation and reload.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        performAction:grey_tap()];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
               @"Last request URL: %@", self.lastRequestURLSpec);

    [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
        performAction:grey_tap()];
    [ChromeEarlGreyUI reload];

    // Makes server respond.
    [self setServerPaused:NO];
  }

  // Verifies that page2 was reloaded.
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last pending URL during
// back forward navigations initiated with JS.
- (void)testJSBackForwardNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the back button on the page and verify that URL1 (pending URL) is
    // displayed.
    [ChromeEarlGrey
        tapWebStateElementWithID:base::SysUTF8ToNSString(kGoBackLink)];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
               @"Last request URL: %@", self.lastRequestURLSpec);

    // Make server respond so URL1 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the forward button on the page and verify that URL2 (pending URL)
    // is displayed.
    [ChromeEarlGrey
        tapWebStateElementWithID:base::SysUTF8ToNSString(kGoForwardLink)];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
               @"Last request URL: %@", self.lastRequestURLSpec);

    // Make server respond so URL2 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last pending URL during go
// navigations initiated with JS.
- (void)testJSGoNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the go negative delta button on the page and verify that URL1
    // (pending URL) is displayed.
    [ChromeEarlGrey
        tapWebStateElementWithID:base::SysUTF8ToNSString(kGoNegativeDeltaLink)];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
               @"Last request URL: %@", self.lastRequestURLSpec);

    // Make server respond so URL1 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap go positive delta button on the page and verify that URL2 (pending
    // URL) is displayed.
    [ChromeEarlGrey
        tapWebStateElementWithID:base::SysUTF8ToNSString(kGoPositiveDeltaLink)];
    GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
               @"Last request URL: %@", self.lastRequestURLSpec);

    // Make server respond so URL2 becomes committed.
    [self setServerPaused:NO];
  }
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL if user
// issues 2 go forward commands to WebUI page (crbug.com/711465).
- (void)testDoubleForwardNavigationToWebUIPage {
  // Create 3rd entry in the history, to be able to go back twice.
  GURL URL(kChromeUIVersionURL);
  [ChromeEarlGrey loadURL:GURL(kChromeUIVersionURL)];

  // Tap the back button twice in the toolbar and wait for URL 1 to load.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];

  // Quickly navigate forward twice and wait for kChromeUIVersionURL to load.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey goForward];

  const std::string version(version_info::GetVersionNumber());
  [ChromeEarlGrey waitForWebStateContainingText:version];

  // Make sure that kChromeUIVersionURL URL is displayed in the omnibox.
  std::string expectedText =
      base::SysNSStringToUTF8([ChromeEarlGrey displayTitleForURL:URL]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last pending URL if page calls
// window.history.back() twice.
- (void)testDoubleBackJSNavigation {
  // Create 3rd entry in the history, to be able to go back twice.
  [ChromeEarlGrey loadURL:_testURL3];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage3];
  {
    // Pauses response server and disables EG synchronization.
    // Pending navigation will not complete until server is unpaused.
    ScopedSynchronizationDisabler disabler;
    [self setServerPaused:YES];

    // Tap the window.history.go(-2) link.
    [ChromeEarlGrey tapWebStateElementWithID:base::SysUTF8ToNSString(
                                                 kGoNegativeDeltaTwiceLink)];

    // Make server respond so URL1 becomes committed.
    [self setServerPaused:NO];
  }

  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

#pragma mark -
#pragma mark Private

- (NSString*)lastRequestURLSpec {
  return base::SysUTF8ToNSString(_responseProvider->last_request_url().spec());
}

- (void)setServerPaused:(BOOL)paused {
  _responseProvider->set_paused(paused);
}

- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL {
  return [[GREYCondition
      conditionWithName:@"Wait for received request"
                  block:^{
                    return self->_responseProvider->last_request_url() == URL;
                  }] waitWithTimeout:10];
}

@end

// Test using synthesized restore.
@interface VisibleURLWithWithSynthesizedRestoreTestCase
    : VisibleURLWithCachedRestoreTestCase
@end

@implementation VisibleURLWithWithSynthesizedRestoreTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(
      web::features::kForceSynthesizedRestoreSession);
  return config;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end
