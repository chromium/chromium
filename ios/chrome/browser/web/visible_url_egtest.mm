// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/features.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/test/url_test_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OmniboxText;
using chrome_test_util::PurgeCachedWebViewPages;

namespace {

const char kTestPage1[] = "Test Page 1";
const char kTestPage2[] = "Test Page 2";
const char kTestPage3[] = "Test Page 3";
const char kGoBackLink[] = "go-back";
const char kGoForwardLink[] = "go-forward";
const char kGoNegativeDeltaLink[] = "go-negative-delta";
const char kGoPositiveDeltaLink[] = "go-positive-delta";
const char kPage1Link[] = "page-1";
const char kPage2Link[] = "page-2";
const char kPage3Link[] = "page-3";

// Response provider which can be paused. When it is paused it buffers all
// requests and does not respond to them until |set_paused(false)| is called.
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
@interface VisibleURLTestCase : ChromeTestCase {
  PausableResponseProvider* _responseProvider;
  GURL _testURL1;
  GURL _testURL2;
  GURL _testURL3;
}

// Spec of the last request URL that reached the server.
@property(nonatomic, copy, readonly) NSString* lastRequestURLSpec;

// Pauses response server and disables EG synchronization if |paused| is YES.
// Pending navigation will not complete until server is unpaused.
- (void)setServerPaused:(BOOL)paused;

// Waits until |_responseProvider| receives a request with the given |URL|.
// Returns YES if request was received, NO on timeout.
- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL;

@end

@implementation VisibleURLTestCase

- (void)setUp {
  [super setUp];

  _testURL1 = web::test::HttpServer::MakeUrl("http://url1.test/");
  _testURL2 = web::test::HttpServer::MakeUrl("http://url2.test/");
  _testURL3 = web::test::HttpServer::MakeUrl("http://url3.test/");

  // Every page has links for window.history navigations (back, forward, go).
  const std::string pageContent = base::StringPrintf(
      "<a onclick='window.history.back()' id='%s'>Go Back</a>"
      "<a onclick='window.history.forward()' id='%s'>Go Forward</a>"
      "<a onclick='window.history.go(-1)' id='%s'>Go Delta -1</a>"
      "<a onclick='window.history.go(1)' id='%s'>Go Delta +1</a>"
      "<a href='%s' id='%s'>Page 1</a>"
      "<a href='%s' id='%s'>Page 2</a>"
      "<a href='%s' id='%s'>Page 3</a>",
      kGoBackLink, kGoForwardLink, kGoNegativeDeltaLink, kGoPositiveDeltaLink,
      _testURL1.spec().c_str(), kPage1Link, _testURL2.spec().c_str(),
      kPage2Link, _testURL3.spec().c_str(), kPage3Link);

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  responses[_testURL1] = std::string(kTestPage1) + pageContent;
  responses[_testURL2] = std::string(kTestPage2) + pageContent;
  responses[_testURL3] = std::string(kTestPage3) + pageContent;

  std::unique_ptr<PausableResponseProvider> unique_provider =
      std::make_unique<PausableResponseProvider>(responses);
  _responseProvider = unique_provider.get();
  web::test::SetUpHttpServer(std::move(unique_provider));

  [ChromeEarlGrey loadURL:_testURL1];
  [ChromeEarlGrey loadURL:_testURL2];
}

- (void)tearDown {
  // This test case disables synchronization, so make sure that it is enabled
  // if that test has failed and did not enable it back.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  [super tearDown];
}

#pragma mark -
#pragma mark Tests

// Tests that visible URL is always the same as last committed URL during
// pending back and forward navigations.
- (void)testBackForwardNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the back button in the toolbar and verify that URL2 (committed URL) is
  // displayed even though URL1 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [self setServerPaused:YES];

  // Tap the forward button in the toolbar and verify that URL1 (committed URL)
  // is displayed even though URL2 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL2 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during
// pending navigations initialted from back history popover.
- (void)testHistoryNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Re-enable synchronization here to synchronize EarlGrey LongPress and Tap
  // actions.
  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  // Go back in history and verify that URL2 (committed URL) is displayed even
  // though URL1 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
  NSString* URL1Title =
      base::SysUTF16ToNSString(web::GetDisplayTitleForUrl(_testURL1));
  [[EarlGrey selectElementWithMatcher:grey_text(URL1Title)]
      performAction:grey_tap()];

  [[GREYConfiguration sharedInstance]
          setValue:@(NO)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that stopping a pending Back navigation and reloading reloads committed
// URL, not pending URL.
- (void)testStoppingPendingBackNavigationAndReload {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the back button, stop pending navigation and reload.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  // On iPhone Stop/Reload button is a part of tools menu, so open it.
  if (!IsIPadIdiom()) {
    // Enable EG synchronization to make test wait for popover animations.
    [[GREYConfiguration sharedInstance]
            setValue:@YES
        forConfigKey:kGREYConfigKeySynchronizationEnabled];
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI reload];

  // Make server respond and verify that page2 was reloaded, not page1.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during
// back forward navigations initiated with JS.
- (void)testJSBackForwardNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the back button on the page and verify that URL2 (committed URL) is
  // displayed even though URL1 is a pending URL.
  [ChromeEarlGrey tapWebViewElementWithID:base::SysUTF8ToNSString(kGoBackLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [self setServerPaused:YES];

  // Tap the forward button on the page and verify that URL1 (committed URL)
  // is displayed even though URL2 is a pending URL.
  [ChromeEarlGrey
      tapWebViewElementWithID:base::SysUTF8ToNSString(kGoForwardLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL2 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during go
// navigations initiated with JS.
- (void)testJSGoNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the go negative delta button on the page and verify that URL2
  // (committed URL) is displayed even though URL1 is a pending URL.
  [ChromeEarlGrey
      tapWebViewElementWithID:base::SysUTF8ToNSString(kGoNegativeDeltaLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [self setServerPaused:YES];

  // Tap go positive delta button on the page and verify that URL1 (committed
  // URL) is displayed even though URL2 is a pending URL.
  [ChromeEarlGrey
      tapWebViewElementWithID:base::SysUTF8ToNSString(kGoPositiveDeltaLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL2 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during go
// back navigation started with pending reload in progress.
- (void)testBackNavigationWithPendingReload {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Start reloading the page.
  if (!IsIPadIdiom()) {
    // Enable EG synchronization to make test wait for popover animations.
    [[GREYConfiguration sharedInstance]
            setValue:@YES
        forConfigKey:kGREYConfigKeySynchronizationEnabled];
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
      performAction:grey_tap()];

  // Do not wait until reload is finished, tap the back button in the toolbar
  // and verify that URL2 (committed URL) is displayed even though URL1 is a
  // pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/724560): Re-evaluate if necessary to check receiving URL1
  // request here.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during go
// back navigation initiated with pending renderer-initiated navigation in
// progress.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testBackNavigationWithPendingRendererInitiatedNavigation \
  testBackNavigationWithPendingRendererInitiatedNavigation
#else
#define MAYBE_testBackNavigationWithPendingRendererInitiatedNavigation \
  FLAKY_testBackNavigationWithPendingRendererInitiatedNavigation
#endif
- (void)MAYBE_testBackNavigationWithPendingRendererInitiatedNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Start renderer initiated navigation.
  [ChromeEarlGrey tapWebViewElementWithID:base::SysUTF8ToNSString(kPage3Link)];

  // Do not wait until renderer-initiated navigation is finished, tap the back
  // button in the toolbar and verify that URL2 (committed URL) is displayed
  // even though URL1 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/724560): Re-evaluate if necessary to check receiving URL1
  // request here.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL during
// renderer-initiated navigation started with pending back navigation in
// progress.
- (void)testRendererInitiatedNavigationWithPendingBackNavigation {
  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the back button in the toolbar and verify that URL2 (committed URL) is
  // displayed even though URL1 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Interrupt back navigation with renderer initiated navigation.
  [ChromeEarlGrey tapWebViewElementWithID:base::SysUTF8ToNSString(kPage3Link)];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage3];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL3.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL if user
// issues 2 go back commands.
- (void)testDoubleBackNavigation {
  // Create 3rd entry in the history, to be able to go back twice.
  [ChromeEarlGrey loadURL:_testURL3];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage3];
  [self setServerPaused:YES];

  // Tap the back button twice in the toolbar and verify that URL3 (committed
  // URL) is displayed even though URL1 is a pending URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  // Server will receive only one request either for |_testURL2| or for
  // |_testURL1| depending on load timing and then will pause. So there is no
  // need to wait for particular request.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
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
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];

  // Quickly navigate forward twice and wait for kChromeUIVersionURL to load.
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() goForward];
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() goForward];

  const std::string version = version_info::GetVersionNumber();
  [ChromeEarlGrey waitForWebViewContainingText:version];

  // Make sure that kChromeUIVersionURL URL is displayed in the omnibox.
  std::string expectedText = base::UTF16ToUTF8(web::GetDisplayTitleForUrl(URL));
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that visible URL is always the same as last committed URL if page calls
// window.history.back() twice.
- (void)testDoubleBackJSNavigation {
  // Create 3rd entry in the history, to be able to go back twice.
  [ChromeEarlGrey loadURL:_testURL3];

  // Purge web view caches and pause the server to make sure that tests can
  // verify omnibox state before server starts responding.
  GREYAssert(PurgeCachedWebViewPages(), @"Pages were not purged");
  [ChromeEarlGrey waitForWebViewContainingText:kTestPage3];
  [self setServerPaused:YES];

  // Tap the back button twice on the page and verify that URL3 (committed URL)
  // is displayed even though URL1 is a pending URL.
  [ChromeEarlGrey tapWebViewElementWithID:base::SysUTF8ToNSString(kGoBackLink)];
  [ChromeEarlGrey tapWebViewElementWithID:base::SysUTF8ToNSString(kGoBackLink)];
  // Server will receive only one request either for |_testURL2| or for
  // |_testURL1| depending on load timing and then will pause. So there is no
  // need to wait for particular request.
  [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make server respond so URL1 becomes committed.
  [self setServerPaused:NO];
  if (base::FeatureList::IsEnabled(web::features::kWebFrameMessaging)) {
    // With frame messaging, only one back is executed. This is the expected
    // behavior.
    [ChromeEarlGrey waitForWebViewContainingText:kTestPage2];
    [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL2.GetContent())]
        assertWithMatcher:grey_notNil()];
  } else {
    // TODO(crbug.com/866406): fix the test to have documented behavior.
    [ChromeEarlGrey waitForWebViewContainingText:kTestPage1];
    [[EarlGrey selectElementWithMatcher:OmniboxText(_testURL1.GetContent())]
        assertWithMatcher:grey_notNil()];
  }
}

#pragma mark -
#pragma mark Private

- (NSString*)lastRequestURLSpec {
  return base::SysUTF8ToNSString(_responseProvider->last_request_url().spec());
}

- (void)setServerPaused:(BOOL)paused {
  // Disable EG synchronization if server is paused so the framework does not
  // wait until the tab loading spinner or progress bar indicator becomes idle
  // (which will not happen until server responds and the navigation is
  // finished).
  [[GREYConfiguration sharedInstance]
          setValue:@(!paused)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  _responseProvider->set_paused(paused);
}

- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL {
  return [[GREYCondition
      conditionWithName:@"Wait for received request"
                  block:^{
                    return _responseProvider->last_request_url() == URL ? YES
                                                                        : NO;
                  }] waitWithTimeout:10];
}

@end
