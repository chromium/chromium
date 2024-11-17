// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/field_trial.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/web_ui_test_utils.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

// Test case for chrome://* WebUI pages.
@interface WebUITestCase : ChromeTestCase
@end

@implementation WebUITestCase

// Tests that the WebUI pages (chrome://version) have the correct User Agent.
- (void)testUserAgent {
  [ChromeEarlGrey loadURL:WebUIPageUrlWithHost(kChromeUIVersionHost)];

  NSString* userAgent = [ChromeEarlGrey mobileUserAgentString];
  // Verify that JavaScript navigator.userAgent returns the mobile User Agent.
  base::Value result =
      [ChromeEarlGrey evaluateJavaScript:@"navigator.userAgent"];
  GREYAssertTrue(result.is_string(), @"Result is not a string.");
  NSString* navigatorUserAgent = base::SysUTF8ToNSString(result.GetString());
  GREYAssertEqualObjects(userAgent, navigatorUserAgent,
                         @"User-Agent strings did not match");
}

// Tests that chrome://version renders and contains correct version number and
// user agent string.
- (void)testVersion {
  [ChromeEarlGrey loadURL:WebUIPageUrlWithHost(kChromeUIVersionHost)];

  // Verify that app version is present on the page.
  const std::string version(version_info::GetVersionNumber());
  [ChromeEarlGrey waitForWebStateContainingText:version];

  NSString* userAgent = [ChromeEarlGrey mobileUserAgentString];
  std::string userAgentString = base::SysNSStringToUTF8(userAgent);

  // Verify that mobile User Agent string is present on the page. Testing for
  // only a portion of the string is sufficient to ensure the value has been
  // populated in the UI and it is not blank. However, the exact string value is
  // not validated as this test does not have access to get the full User Agent
  // string from the WebClient.
  [ChromeEarlGrey waitForWebStateContainingText:userAgentString];
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
  const std::string kTermsText = "Terms of Service";
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
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_ABOUT_VERSION_COMPANY_NAME)];

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

  std::string chromeVersionWebText =
      l10n_util::GetStringUTF8(IDS_IOS_ABOUT_VERSION_COMPANY_NAME);
  const char kWebPageText[] = "pony";

  [ChromeEarlGrey loadURL:GURL(kChromeUIVersionURL)];

  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:chromeVersionWebText];

  GURL webURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:webURL];
  [ChromeEarlGrey waitForWebStateContainingText:kWebPageText];

  [ChromeEarlGrey goBack];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:chromeVersionWebText];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:kWebPageText];

  [ChromeEarlGrey goBack];
  GREYAssert(WaitForOmniboxURLString(kChromeUIVersionURL),
             @"Omnibox did not contain URL.");
  [ChromeEarlGrey waitForWebStateContainingText:chromeVersionWebText];
}

- (void)testChromeFlagsOnNTP {
  // Start with NTP and load chrome://flags.
  [ChromeEarlGrey loadURL:GURL(kChromeUIFlagsURL)];

  GREYAssert(WaitForOmniboxURLString(kChromeUIFlagsURL),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Experiments"];
  NSString* selector =
      @"(function() {"
       "  var app = document.body.querySelector('flags-app');"
       "  var crTabs = app.shadowRoot.querySelector('cr-tabs');"
       "  return crTabs.tabNames.includes('Available');"
       "})()";
  NSString* description = @"'Available' tab exists.";
  ElementSelector* tabTextSelector =
      [ElementSelector selectorWithScript:selector
                      selectorDescription:description];
  [ChromeEarlGrey waitForWebStateContainingElement:tabTextSelector];

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
  NSString* selector =
      @"(function() {"
       "  var app = document.body.querySelector('flags-app');"
       "  var crTabs = app.shadowRoot.querySelector('cr-tabs');"
       "  return crTabs.tabNames.includes('Available');"
       "})()";
  NSString* description = @"'Available' tab exists.";
  ElementSelector* tabTextSelector =
      [ElementSelector selectorWithScript:selector
                      selectorDescription:description];
  [ChromeEarlGrey waitForWebStateContainingElement:tabTextSelector];

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

  // Autofill-Internals stores the log filter configuration in the URL's
  // fragment identifier (after the hash).
  GREYAssert(WaitForOmniboxURLString(URL.spec(), false),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Variations"];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Internals"];
}

// Test chrome://userdefaults-internals page.
- (void)testChromeUserdefaultsInternalSite {
  if (GetChannel() == version_info::Channel::STABLE) {
    // This page is not supported on STABLE build.
    return;
  }

  // Start with NTP and load chrome://userdefaults-internals.
  GURL URL = WebUIPageUrlWithHost(kChromeUIUserDefaultsInternalsHost);
  [ChromeEarlGrey loadURL:URL];

  // Autofill-Internals stores the log filter configuration in the URL's
  // fragment identifier (after the hash).
  GREYAssert(WaitForOmniboxURLString(URL.spec()),
             @"Omnibox did not contain URL");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"List of user defaults:"];
}

// Test chrome://userdefaults-internals page in incognito.
- (void)testChromeUserdefaultsInternalIncognitoSite {
  if (GetChannel() == version_info::Channel::STABLE) {
    // This page is not supported on STABLE build.
    return;
  }

  // Start with incognito NTP and load chrome://userdefaults-internals.
  GURL URL = WebUIPageUrlWithHost(kChromeUIUserDefaultsInternalsHost);
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];

  // Autofill-Internals stores the log filter configuration in the URL's
  // fragment identifier (after the hash).
  GREYAssert(WaitForOmniboxURLString(URL.spec()),
             @"Omnibox did not contain URL");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:
                      "This page is not available in incognito mode."];
}

@end
