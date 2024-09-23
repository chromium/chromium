// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#import "base/base_paths.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using chrome_test_util::OmniboxText;
using chrome_test_util::NTPCollectionView;
using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

namespace {

// Path to two test pages, page1 and page2 with associated contents and titles.
const char kPageOnePath[] = "/page1.html";
const char kPageOneContent[] = "This is the first page.";
const char kPageOneTitle[] = "The first page title.";
const char kPageTwoPath[] = "/page2.html";
const char kPageTwoContent[] = "This is the second page.";
const char kPageTwoTitle[] = "The second page title.";

// Path to a test page used to count each page load.
const char kCountURL[] = "/countme.html";

// Response handler for page1 and page2 that supports 'airplane mode' by
// returning an empty RawHttpResponse when `responds_with_content` us false.
std::unique_ptr<net::test_server::HttpResponse> RestoreResponse(
    const bool& responds_with_content,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  std::string title;
  std::string body;
  if (request.relative_url == kPageOnePath) {
    title = kPageOneTitle;
    body = kPageOneContent;
  } else if (request.relative_url == kPageTwoPath) {
    title = kPageTwoTitle;
    body = kPageTwoContent;
  } else {
    return nullptr;
  }
  http_response->set_content("<html><head><title>" + title +
                             "</title></head>"
                             "<body>" +
                             body + "</body></html>");
  return std::move(http_response);
}

// Response handler for `kCountURL` that counts the number of page loads.
std::unique_ptr<net::test_server::HttpResponse> CountResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kCountURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>Hello World</title></head>"
                             "<body>Hello World!</body></html>");
  (*counter)++;
  return std::move(http_response);
}

// Returns true when omnibox contains `text`, otherwise returns false after
// after a timeout.
[[nodiscard]] bool WaitForOmniboxContaining(std::string text) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:OmniboxText(text)]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return error == nil;
      });
}
}

// Integration tests for restoring session history.
@interface RestoreWithCacheTestCase : ChromeTestCase {
  // Use a second test server to ensure different origin navigations.
  std::unique_ptr<net::EmbeddedTestServer> _secondTestServer;
}

// The secondary EmbeddedTestServer instance.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* secondTestServer;

@property(atomic) bool serverRespondsWithContent;

// Start the primary and secondary test server.  Separate servers are used to
// force cross domain tests (via different ports).
- (void)setUpRestoreServers;

// Trigger a session history restore.  In EG1 this is possible via the TabGrid
// CloseAll-Undo-Done method. In EG2, this is possible via
// Background-Terminate-Activate
- (void)triggerRestore;

// Navigate to a set of sites include cross-domains, chrome URLs, error pages
// and the NTP.
- (void)loadTestPages;

// Verify that each page visited in -loadTestPages is properly restored by
// navigating to each page and triggering a restore, confirming that pages are
// reloaded and back-forward history is preserved.  If `checkServerData` is YES,
// also check that the proper content is restored.
- (void)verifyRestoredTestPages:(BOOL)checkServerData;

@end

@implementation RestoreWithCacheTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  return config;
}

- (net::EmbeddedTestServer*)secondTestServer {
  if (!_secondTestServer) {
    _secondTestServer = std::make_unique<net::EmbeddedTestServer>();
    _secondTestServer->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("ios/testing/data/http_server_files/"));
    net::test_server::RegisterDefaultHandlers(_secondTestServer.get());
  }
  return _secondTestServer.get();
}

// Navigates to a set of cross-domains, chrome URLs and error pages, and then
// tests that they are properly restored.
- (void)testRestoreHistory {
  [self setUpRestoreServers];
  [self loadTestPages];
  [self verifyRestoredTestPages:YES];
}

// Navigates to a set of cross-domains, chrome URLs and error pages, and then
// tests that they are properly restored in airplane mode.
- (void)testRestoreNoNetwork {
  [self setUpRestoreServers];
  [self loadTestPages];
  self.serverRespondsWithContent = false;
  [self verifyRestoredTestPages:NO];
}

// Tests that only the selected web state is loaded on a session restore.
- (void)testRestoreOneWebstateOnly {
  // Visit the background page.
  int visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&CountResponse, &visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL countPage = self.testServer->GetURL(kCountURL);
  [ChromeEarlGrey loadURL:countPage];
  GREYAssertEqual(1, visitCounter, @"The page should have been loaded once");

  // Visit the forground page.
  [ChromeEarlGrey openNewTab];
  const GURL echoPage = self.testServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:echoPage];

  // Trigger a restore and confirm the background page is not reloaded.
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(echoPage.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertEqual(1, visitCounter, @"The page should not reload");
}

// Tests that only the selected web state is loaded Restore-after-Crash.  This
// is only possible in EG2.
- (void)testRestoreOneWebstateOnlyAfterCrash {
  // Visit the background page.
  int visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&CountResponse, &visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL countPage = self.testServer->GetURL(kCountURL);
  [ChromeEarlGrey loadURL:countPage];
  GREYAssertEqual(1, visitCounter, @"The page should have been loaded once");

  // Visit the foreground page.
  [ChromeEarlGrey openNewTab];
  const GURL echoPage = self.testServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:echoPage];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Clear cache, save the session and trigger a crash/activate.
  [ChromeEarlGrey removeBrowsingCache];
  [ChromeEarlGrey saveSessionImmediately];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByKilling];
  // Restore after crash and confirm the background page is not reloaded.
  [[EarlGrey selectElementWithMatcher:OmniboxText(echoPage.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertEqual(1, visitCounter, @"The page should not reload");
}

#pragma mark Utility methods

- (void)setUpRestoreServers {
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &RestoreResponse, std::cref(_serverRespondsWithContent)));
  self.secondTestServer->RegisterRequestHandler(base::BindRepeating(
      &RestoreResponse, std::cref(_serverRespondsWithContent)));
  self.serverRespondsWithContent = true;
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GREYAssertTrue(self.secondTestServer->Start(),
                 @"Second test server failed to start.");
}

- (void)triggerRestore {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{}
                                  disabled:{kStartSurface}
                            relaunchPolicy:ForceRelaunchByCleanShutdown];
}

- (void)loadTestPages {
  // Load page1.
  const GURL pageOne = self.testServer->GetURL(kPageOnePath);
  [ChromeEarlGrey loadURL:pageOne];
  [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent];

  // Load chrome url
  const GURL chromePage = GURL("chrome://chrome-urls");
  [ChromeEarlGrey loadURL:chromePage];

  // Load error page.
  const GURL errorPage = GURL("http://invalid.");
  [ChromeEarlGrey loadURL:errorPage];
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_"];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Load page2.
  const GURL pageTwo = self.secondTestServer->GetURL(kPageTwoPath);
  [ChromeEarlGrey loadURL:pageTwo];
  [ChromeEarlGrey waitForWebStateContainingText:kPageTwoContent];
}

- (void)verifyRestoredTestPages:(BOOL)checkServerData {
  const GURL pageOne = self.testServer->GetURL(kPageOnePath);
  const GURL pageTwo = self.secondTestServer->GetURL(kPageTwoPath);

  // Restore page2
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageTwo.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageTwoContent];
  }

  // Confirm page1 is still in the history.
  [[EarlGrey selectElementWithMatcher:BackButton()]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(base::SysUTF8ToNSString(
                                              kPageOneTitle)),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];

  // Go back to error page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  GREYAssert(
      WaitForOmniboxContaining("invalid."),
      @"Timeout while waiting for  omnibox text to become \"invalid.\".");
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_"];
  [ChromeEarlGreyUI waitForAppToIdle];
  [self triggerRestore];
  GREYAssert(
      WaitForOmniboxContaining("invalid."),
      @"Timeout while waiting for  omnibox text to become \"invalid.\".");
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_"];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Go back to chrome url.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  GREYAssert(WaitForOmniboxContaining("chrome://chrome-urls"),
             @"Timeout while waiting for  omnibox text to become "
             @"\"chrome://chrome-urls\".");
  [ChromeEarlGrey waitForWebStateContainingText:"List of Chrome"];
  [self triggerRestore];
  GREYAssert(WaitForOmniboxContaining("chrome://chrome-urls"),
             @"Timeout while waiting for  omnibox text to become "
             @"\"chrome://chrome-urls\".");
  [ChromeEarlGrey waitForWebStateContainingText:"List of Chrome"];

  // Go back to page1 and confirm page2 is still in the forward history.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageOne.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_longPress()];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(base::SysUTF8ToNSString(
                                                kPageTwoTitle)),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_tap()];
  }
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageOne.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_longPress()];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(base::SysUTF8ToNSString(
                                                kPageTwoTitle)),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Confirm the NTP is still at the start.
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
}

@end

// Test using synthesize restore.
@interface RestoreWithSynthesizedTestCase : RestoreWithCacheTestCase
@end

@implementation RestoreWithSynthesizedTestCase

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
