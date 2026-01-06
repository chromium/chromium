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
#import "base/synchronization/lock.h"
#import "base/synchronization/waitable_event.h"
#import "base/test/test_waitable_event.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_earl_grey.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

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
class PausableRequestHandler {
 public:
  PausableRequestHandler()
      : unpause_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::SIGNALED) {}

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    {
      base::AutoLock lock(lock_);
      last_request_url_ = request.GetURL();
    }
    unpause_event_.Wait();

    std::string path(request.GetURL().path());
    if (path.size() > 0 && path[0] == '/') {
      path = path.substr(1);
    }

    const char* title = nullptr;
    if (path == kPage1Link) {
      title = kTestPage1;
    } else if (path == kPage2Link) {
      title = kTestPage2;
    } else if (path == kPage3Link) {
      title = kTestPage3;
    }

    if (!title) {
      return nullptr;
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(GetPageContent(title));
    return response;
  }

  // URL for the last seen request.
  const GURL& GetLastRequestUrl() const {
    base::AutoLock autolock(lock_);
    return last_request_url_;
  }

  void SetPaused(bool paused) {
    if (paused) {
      unpause_event_.Reset();
    } else {
      unpause_event_.Signal();
    }
  }

 private:
  std::string GetPageContent(const char* title) {
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
        kGoNegativeDeltaTwiceLink, kGoPositiveDeltaLink, kPage1Link, kPage1Link,
        kPage2Link, kPage2Link, kPage3Link, kPage3Link);
  }

  mutable base::Lock lock_;
  GURL last_request_url_ GUARDED_BY(lock_);
  base::TestWaitableEvent unpause_event_;
};

}  // namespace

// Test case for back forward and delta navigations focused on making sure that
// omnibox visible URL always represents the current page.
@interface VisibleURLWithCachedRestoreTestCase : ChromeTestCase {
  std::unique_ptr<PausableRequestHandler> _requestHandler;
  GURL _testURL1;
  GURL _testURL2;
  GURL _testURL3;
}

// Spec of the last request URL that reached the server.
@property(nonatomic, copy, readonly) NSString* lastRequestURLSpec;

// Pauses response server.
- (void)setServerPaused:(BOOL)paused;

// Waits until `_requestHandler` receives a request with the given `URL`.
// Returns YES if request was received, NO on timeout.
- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL;

@end

@implementation VisibleURLWithCachedRestoreTestCase

- (void)setUp {
  [super setUp];
  _requestHandler = std::make_unique<PausableRequestHandler>();
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PausableRequestHandler::HandleRequest,
                          base::Unretained(_requestHandler.get())));
  GREYAssertTrue(self.testServer->Start(), @"Server failed to start.");

  _testURL1 = self.testServer->GetURL(std::string("/") + kPage1Link);
  _testURL2 = self.testServer->GetURL(std::string("/") + kPage2Link);
  _testURL3 = self.testServer->GetURL(std::string("/") + kPage3Link);

  [ChromeEarlGrey loadURL:_testURL1];
  [ChromeEarlGrey loadURL:_testURL2];
}

#pragma mark - Tests

// Tests that visible URL is always the committed URL during
// pending back and forward navigations.
- (void)testBackForwardNavigation {
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];

  // Tap the back button in the toolbar and verify that URL1 is displayed.
  [self setServerPaused:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL2.GetContent()];

  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [self assertFocusedOmniboxText:_testURL1.GetContent()];

  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];

  // Tap the forward button in the toolbar and verify that URL2 is displayed.
  [self setServerPaused:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL1.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
}

// Tests that visible URL is always the comitted URL during
// navigations initiated from back history popover.
- (void)testHistoryNavigation {
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  // Go back in history and verify that URL1 is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
  NSString* URL1Title = base::SysUTF8ToNSString(kTestPage1);
  [self setServerPaused:YES];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     URL1Title)] performAction:grey_tap()];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [self assertFocusedOmniboxText:_testURL1.GetContent()];
}

// Tests that stopping a pending Back navigation and reloading reloads the
// committed URL.
- (void)testStoppingPendingBackNavigationAndReload {
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [self setServerPaused:YES];

  // Tap the back button, stop pending navigation and reload.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  if (![ChromeEarlGrey isIPadIdiom]) {
    // On iPhone Stop/Reload button is a part of tools menu, so open it.
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/467001192): This should also check OmniboxText
  [ChromeEarlGreyUI reload];

  // Makes server respond.
  [self setServerPaused:NO];

  // Verifies that page2 was reloaded.
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
}

// Tests that visible URL correct during back forward navigations initiated with
// JS.
- (void)testJSBackForwardNavigation {
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];

  // Tap the back button on the page and verify that URL2 (committed URL) is
  // displayed.
  [self setServerPaused:YES];
  [ChromeEarlGrey
      tapWebStateElementWithID:base::SysUTF8ToNSString(kGoBackLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [self assertFocusedOmniboxText:_testURL1.GetContent()];

  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];

  // Tap the forward button on the page and verify that URL1 (committed URL)
  // is displayed.
  [self setServerPaused:YES];
  [ChromeEarlGrey
      tapWebStateElementWithID:base::SysUTF8ToNSString(kGoForwardLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL1.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
}

// Tests that visible URL is always correct with go navigations initiated with
// JS.
- (void)testJSGoNavigation {
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];

  // Tap the go negative delta button on the page and verify that URL2
  // (committed URL) is displayed.
  [self setServerPaused:YES];
  [ChromeEarlGrey
      tapWebStateElementWithID:base::SysUTF8ToNSString(kGoNegativeDeltaLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [self assertFocusedOmniboxText:_testURL1.GetContent()];

  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];

  // Tap go positive delta button on the page and verify that URL1 (committed
  // URL) is displayed.
  [self setServerPaused:YES];
  [ChromeEarlGrey
      tapWebStateElementWithID:base::SysUTF8ToNSString(kGoPositiveDeltaLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL2],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL1.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage2];
  [self assertFocusedOmniboxText:_testURL2.GetContent()];
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
  [self assertFocusedOmniboxText:expectedText];
}

// Tests calling window.history.back() twice.
- (void)testDoubleBackJSNavigation {
  // Create 3rd entry in the history, to be able to go back twice.
  [ChromeEarlGrey loadURL:_testURL3];
  [ChromeEarlGrey purgeCachedWebViewPages];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage3];

  // Tap the window.history.go(-2) link.
  [self setServerPaused:YES];
  [ChromeEarlGrey tapWebStateElementWithID:base::SysUTF8ToNSString(
                                               kGoNegativeDeltaTwiceLink)];
  GREYAssert([self waitForServerToReceiveRequestWithURL:_testURL1],
             @"Last request URL: %@", self.lastRequestURLSpec);
  [self assertFocusedOmniboxText:_testURL3.GetContent()];
  [self setServerPaused:NO];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPage1];
  [self assertFocusedOmniboxText:_testURL1.GetContent()];
}

#pragma mark - Private

- (NSString*)lastRequestURLSpec {
  return base::SysUTF8ToNSString(_requestHandler->GetLastRequestUrl().spec());
}

- (void)setServerPaused:(BOOL)paused {
  _requestHandler->SetPaused(paused);
}

- (BOOL)waitForServerToReceiveRequestWithURL:(GURL)URL {
  return [[GREYCondition
      conditionWithName:@"Wait for received request"
                  block:^{
                    return self->_requestHandler->GetLastRequestUrl() == URL;
                  }] waitWithTimeout:10];
}

- (void)assertFocusedOmniboxText:(const std::string&)expectedText {
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(expectedText)];
  [OmniboxEarlGrey defocusOmnibox];
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
