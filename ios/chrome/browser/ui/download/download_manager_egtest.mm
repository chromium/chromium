// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::WebViewMatcher;

namespace {

// Matcher for "Download" button on Download Manager UI.
id<GREYMatcher> DownloadButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD);
}

// Provides downloads landing page with download link.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<a id='download' href='/download-example?50000'>Download</a>");
  return result;
}

// Provides test page for downloads with content disposition
std::unique_ptr<net::test_server::HttpResponse> GetContentDispositionResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<a id='pdf' download='test.pdf' href='/single_page_wide.pdf'>PDF</a>");
  result->set_content_type("attachment; filename='filename.pdf'");
  return result;
}

// Waits until Open in... button is shown.
[[nodiscard]] bool WaitForOpenInButton() {
  // These downloads usually take longer and need a longer timeout.
  constexpr base::TimeDelta kLongDownloadTimeout = base::Minutes(1);
  return base::test::ios::WaitUntilConditionOrTimeout(kLongDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
        assertWithMatcher:grey_interactable()
                    error:&error];
    return (error == nil);
  });
}

// Waits until Download button is shown.
[[nodiscard]] bool WaitForDownloadButton() {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:DownloadButton()]
            assertWithMatcher:grey_interactable()
                        error:&error];
        return (error == nil);
      });
}

}  // namespace

// Tests critical user journeys for Download Manager with Save to Drive
// disabled.
@interface DownloadManagerTestCase : ChromeTestCase
@end

@implementation DownloadManagerTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&GetResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/content-disposition",
      base::BindRepeating(&GetContentDispositionResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/download-example",
      base::BindRepeating(&testing::HandleDownload)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  configuration.features_disabled.push_back(kIOSSaveToDrive);
  return configuration;
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process.
- (void)testSuccessfulDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. Performs download in Incognito.
#if !TARGET_IPHONE_SIMULATOR
// TODO(crbug.com/1059320): Test consistently failing on device.
#define MAYBE_testSuccessfulDownloadInIncognito \
  DISABLED_testSuccessfulDownloadInIncognito
#else
#define MAYBE_testSuccessfulDownloadInIncognito \
  testSuccessfulDownloadInIncognito
#endif
- (void)MAYBE_testSuccessfulDownloadInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests cancelling download UI.
- (void)testCancellingDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::CloseButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_nil()];
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. After tapping Download this test opens a
// separate tabs and loads the URL there. Then closes the tab and waits for
// the download completion.
- (void)testDownloadWhileBrowsing {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  {
    // In order to open a new Tab, disable EG synchronization so the framework
    // does not wait until the download progress bar becomes idle (which will
    // not happen until the download is complete).
    ScopedSynchronizationDisabler disabler;
    [ChromeEarlGrey openNewTab];
  }

  // Load a URL in a separate Tab and close that tab.
  [ChromeEarlGrey loadURL:GURL(kChromeUITermsURL)];
  const char kTermsText[] = "Terms of Service";
  [ChromeEarlGrey waitForWebStateContainingText:kTermsText];
  [ChromeEarlGrey closeCurrentTab];
  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests "Open in New Tab" on download link.
- (void)testDownloadInNewTab {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];

  // Open context menu for download link.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:"download"],
                        /*menu_should_appear=*/true)];

  // Tap "Open In New Tab".
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  // Wait until the new tab is open and switch to that tab.
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:1U];
  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");

  // Proceed with download.
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests accessibility on Download Manager UI when download is not started.
- (void)testAccessibilityOnNotStartedDownloadToolbar {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on Download Manager UI when download is complete.
- (void)testAccessibilityOnCompletedDownloadToolbar {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that filename label and "Open in Downloads" button are showing.
- (void)testVisibleFileNameAndOpenInDownloads {
  // Apple is hiding UIActivityViewController's contents from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom])
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      verifyTextVisibleInActivitySheetWithID:l10n_util::GetNSString(
                                                 IDS_IOS_OPEN_IN_DOWNLOADS)];

  // Tests filename label.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(@"download-example"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests successful blob download. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulBlobDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"BlobURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"blob"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests that a pdf can be downloaded. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulPDFDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests that a file is downloaded successfully even if it is renderable by the
// browser.
- (void)testSuccessfulDownloadWithContentDisposition {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/content-disposition")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

@end
