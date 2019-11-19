// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/mac/foundation_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::TrimPositions;
using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::OmniboxText;

namespace {

// Returns the url to the web ui page |host|. |url::SchemeHostPort| can not be
// used when this test is run using EarlGrey2 because the chrome scheme is not
// registered in the test process and |url::SchemeHostPort| will not build an
// invalid URL.
GURL WebUIPageUrlWithHost(const std::string& host) {
  return GURL(base::StringPrintf("%s://%s", kChromeUIScheme, host.c_str()));
}

// Waits for omnibox text to equal |URL| and returns true if it was found or
// false on timeout. Strips trailing URL slash if present as the omnibox does
// not display them.
bool WaitForOmniboxURLString(std::string URL) {
  const std::string trimmed_URL =
      base::TrimString(URL, "/", TrimPositions::TRIM_TRAILING).as_string();

  // TODO(crbug.com/642207): Unify with the omniboxText matcher or move to the
  // same location with the omniboxText matcher.
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:OmniboxText(trimmed_URL)]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return error == nil;
      });
}

}  // namespace

// Test case for chrome://* WebUI pages.
@interface WebUITestCase : ChromeTestCase
@end

@implementation WebUITestCase

// Tests that chrome://version renders and contains correct version number and
// user agent string.
- (void)testVersion {
  [ChromeEarlGrey loadURL:WebUIPageUrlWithHost(kChromeUIVersionHost)];

  // Verify that app version is present on the page.
  const std::string version = version_info::GetVersionNumber();
  [ChromeEarlGrey waitForWebStateContainingText:version];

  // Verify that mobile User Agent string is present on the page. Testing for
  // only a portion of the string is sufficient to ensure the value has been
  // populated in the UI and it is not blank. However, the exact string value is
  // not validated as this test does not have access to get the full User Agent
  // string from the WebClient.
  [ChromeEarlGrey waitForWebStateContainingText:"AppleWebKit"];
}

// Tests that clicking on a chrome://terms link from chrome://chrome-urls
// navigates to terms page.
- (void)testChromeURLNavigateToTerms {
  [ChromeEarlGrey loadURL:WebUIPageUrlWithHost(kChromeUIChromeURLsHost)];

  // Tap on chrome://terms link on the page.
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kChromeUITermsHost]];

  // Verify that the resulting page is chrome://terms.
  GREYAssert(WaitForOmniboxURLString(kChromeUITermsURL),
             @"Omnibox does not contain URL.");
  const std::string kTermsText = "Google Chrome Terms of Service";
  [ChromeEarlGrey waitForWebStateContainingText:kTermsText];
}

// Tests that back navigation functions properly after navigation via anchor
// click.
- (void)testChromeURLBackNavigationFromAnchorClick {
  [ChromeEarlGrey loadURL:GURL(kChromeUIChromeURLsURL)];

  // Tap on chrome://version link on the page.
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kChromeUIVersionHost]];

  // Verify that the resulting page is chrome://version.
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:"The Chromium Authors"];

  // Tap the back button in the toolbar and verify that the resulting page is
  // the previously visited page chrome://chrome-urls.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  GREYAssert(WaitForOmniboxURLString(kChromeUIChromeURLsURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:"List of Chrome URLs"];
}

// Tests that back and forward navigation between chrome URLs functions
// properly.
- (void)testChromeURLBackAndForwardAndReloadNavigation {
  // Navigate to the first URL chrome://version.
  [ChromeEarlGrey loadURL:GURL(kChromeUIVersionURL)];

  // Navigate to the second URL chrome://chrome-urls.
  [ChromeEarlGrey loadURL:GURL(kChromeUIChromeURLsURL)];

  // Tap the back button in the toolbar and verify that the resulting page's URL
  // corresponds to the first URL chrome://version that was loaded.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");

  // Tap the forward button in the toolbar and verify that the resulting page's
  // URL corresponds the second URL chrome://chrome-urls that was loaded.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOmniboxURLString(kChromeUIChromeURLsURL),
             @"Omnibox did not contain URL.");

  // Tap the back button in the toolbar then reload, and verify that the
  // resulting page corresponds to the first URL.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey reload];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");

  // Make sure forward navigation is still possible.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOmniboxURLString(kChromeUIChromeURLsURL),
             @"Omnibox did not contain URL.");
}

// Tests that all URLs on chrome://chrome-urls page load without error.
- (void)testChromeURLsLoadWithoutError {
  // Load WebUI pages and verify they load without any error.
  for (size_t i = 0; i < kNumberOfChromeHostURLs; ++i) {
    const char* host = kChromeHostURLs[i];
    // Exclude non-WebUI pages, as they do not go through a "loading" phase.
    if (host == kChromeUINewTabHost) {
      continue;
    }
    GURL URL = WebUIPageUrlWithHost(host);
    [ChromeEarlGrey loadURL:URL];

    GREYAssert(WaitForOmniboxURLString(URL.spec()),
               @"Omnibox did not contain URL.");
  }
}

// Tests that loading an invalid Chrome URL results in an error page.
- (void)testChromeURLInvalid {
  // Navigate to the native error page chrome://invalidchromeurl.
  const std::string kChromeInvalidURL = "chrome://invalidchromeurl";
  [ChromeEarlGrey loadURL:GURL(kChromeInvalidURL)];

  // Verify that the resulting page is an error page.
  GREYAssert(WaitForOmniboxURLString(kChromeInvalidURL),
             @"Omnibox did not contain URL.");
  std::string errorMessage = net::ErrorToShortString(net::ERR_INVALID_URL);
  [ChromeEarlGrey waitForWebStateContainingText:errorMessage];
}

// Tests that repeated back/forward navigation from web URL is allowed.
- (void)testBackForwardFromWebURL {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  const char kChromeVersionWebText[] = "The Chromium Authors";
  const char kWebPageText[] = "pony";

  [ChromeEarlGrey loadURL:GURL(kChromeUIVersionURL)];

  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:kChromeVersionWebText];

  GURL webURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:webURL];
  [ChromeEarlGrey waitForWebStateContainingText:kWebPageText];

  [ChromeEarlGrey goBack];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:kChromeVersionWebText];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:kWebPageText];

  [ChromeEarlGrey goBack];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:kChromeVersionWebText];
}

- (void)testChromeFlagsOnNTP {
  // Start with NTP and load chrome://flags.
  [ChromeEarlGrey loadURL:GURL(kChromeUIFlagsURL)];

  GREYAssert(WaitForOmniboxURLString(kChromeUIFlagsURL),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Experiments"];
  [ChromeEarlGrey waitForWebStateContainingText:"Available"];

  // Validates that the experimental flags container is visible.
  NSString* flags_page_warning =
      l10n_util::GetNSString(IDS_FLAGS_UI_PAGE_WARNING);
  [ChromeEarlGrey waitForWebStateContainingText:base::SysNSStringToUTF8(
                                                    flags_page_warning)];
}

- (void)testChromeFlagsOnWebsite {
  // Starts with loading a website.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  const char kWebPageText[] = "pony";
  GURL webURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:webURL];
  [ChromeEarlGrey waitForWebStateContainingText:kWebPageText];

  // Then load chrome://flags in the same tab that has loaded a website.
  [ChromeEarlGrey loadURL:WebUIPageUrlWithHost(kChromeUIFlagsHost)];

  GREYAssert(WaitForOmniboxURLString(kChromeUIFlagsURL),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Experiments"];
  [ChromeEarlGrey waitForWebStateContainingText:"Available"];

  NSString* flags_page_warning =
      l10n_util::GetNSString(IDS_FLAGS_UI_PAGE_WARNING);
  [ChromeEarlGrey waitForWebStateContainingText:base::SysNSStringToUTF8(
                                                    flags_page_warning)];
}

- (void)testChromePasswordManagerInternalsSite {
  GURL URL = WebUIPageUrlWithHost(kChromeUIPasswordManagerInternalsHost);
  [ChromeEarlGrey loadURL:URL];

  GREYAssert(WaitForOmniboxURLString(URL.spec()),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Variations"];
  [ChromeEarlGrey waitForWebStateContainingText:"Password Manager Internals"];
}

- (void)testChromeAutofillInternalsSite {
  GURL URL = WebUIPageUrlWithHost(kChromeUIAutofillInternalsHost);
  [ChromeEarlGrey loadURL:URL];

  GREYAssert(WaitForOmniboxURLString(URL.spec()),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Variations"];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Internals"];
}

@end
