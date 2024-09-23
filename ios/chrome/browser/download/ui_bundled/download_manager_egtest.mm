// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_constants.h"
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
  return grey_accessibilityID(kDownloadManagerDownloadAccessibilityIdentifier);
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

// Provides test page for new page downloads with content disposition.
std::unique_ptr<net::test_server::HttpResponse>
GetLinkToContentDispositionResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<a id='pdf' download href='/content-disposition'>PDF</a><br/><a "
      "id='pdf_new_window' target='_blank' href='/content-disposition'>PDF in "
      "new tab</a>");
  return result;
}

// Provides test page for downloads with content disposition.
std::unique_ptr<net::test_server::HttpResponse>
GetContentDispositionPDFResponse(const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content("fakePDFData");
  result->AddCustomHeader("Content-Type", "application/pdf");
  result->AddCustomHeader("Content-Disposition",
                          "attachment; filename=filename.pdf");
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

// Waits until `OPEN` button is shown.
[[nodiscard]] bool WaitForOpenPDFButton() {
  // These downloads usually take longer and need a longer timeout.
  constexpr base::TimeDelta kLongDownloadTimeout = base::Minutes(1);
  return base::test::ios::WaitUntilConditionOrTimeout(kLongDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenPDFButton()]
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

// Helper to test critical user journeys for Download Manager.
@interface DownloadManagerTestCaseHelper : NSObject

// The EmbeddedTestServer instance that serves HTTP requests for tests.
@property(nonatomic, assign) net::test_server::EmbeddedTestServer* testServer;

@end

@implementation DownloadManagerTestCaseHelper

- (void)setUp {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&GetResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/link-to-content-disposition",
      base::BindRepeating(&GetLinkToContentDispositionResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/content-disposition",
      base::BindRepeating(&GetContentDispositionPDFResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/download-example",
      base::BindRepeating(&testing::HandleDownload)));

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("ios/testing/data/http_server_files/"));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
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
- (void)testSuccessfulDownloadInIncognito {
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

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kDownloadManagerCloseButtonAccessibilityIdentifier)]
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

// Tests that "Open in..." works if the download ended while waiting in a
// different tab which also contains a download task.
- (void)testSwitchTabsAndOpenInDownloads {
  // Apple is hiding UIActivityViewController's contents from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }

  // Create a download A task in one tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");

  // Go to a second tab and start a download B.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Go back to first tab and wait enough time for download B to complete.
  [ChromeEarlGrey selectTabAtIndex:0];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(10));

  // Go back to second tab and tap "Open in..." for download B.
  [ChromeEarlGrey selectTabAtIndex:1];
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
// downloaded and saved locally while an anchor tag has the download
// attribute.The `shouldOpen` used to wait for the right button once the
// download button is tapped.
- (void)testSuccessfulPDFDownload:(BOOL)shouldOpen {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  if (shouldOpen) {
    GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
  } else {
    GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  }
}

// Tests that a file is downloaded successfully even if it is renderable by the
// browser.The `shouldOpen` used to wait for the right button once the download
// button is tapped.
- (void)testSuccessfulDownloadWithContentDisposition:(BOOL)shouldOpen {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/link-to-content-disposition")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  if (shouldOpen) {
    GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
  } else {
    GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  }
}

// Tests that a file is downloaded successfully when opened in a new window.
- (void)testSuccessfulDownloadWithContentDispositionInNewWindow {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/link-to-content-disposition")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf_new_window"];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
}

@end

// Tests for critical user journeys for Download Manager, without Save to Drive.
@interface DownloadManagerTestCase : ChromeTestCase
@end

@implementation DownloadManagerTestCase {
  DownloadManagerTestCaseHelper* _helper;
}

- (void)setUp {
  [super setUp];
  _helper = [[DownloadManagerTestCaseHelper alloc] init];
  _helper.testServer = self.testServer;
  [_helper setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  configuration.features_disabled.push_back(kIOSSaveToDrive);
  configuration.features_disabled.push_back(kDownloadedPDFOpening);
  return configuration;
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process.
- (void)testSuccessfulDownload {
  [_helper testSuccessfulDownload];
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. Performs download in Incognito.
#if !TARGET_IPHONE_SIMULATOR
// TODO(crbug.com/40678419): Test consistently failing on device.
#define MAYBE_testSuccessfulDownloadInIncognito \
  DISABLED_testSuccessfulDownloadInIncognito
#else
#define MAYBE_testSuccessfulDownloadInIncognito \
  testSuccessfulDownloadInIncognito
#endif
- (void)MAYBE_testSuccessfulDownloadInIncognito {
  [_helper testSuccessfulDownloadInIncognito];
}

// Tests cancelling download UI.
- (void)testCancellingDownload {
  [_helper testCancellingDownload];
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. After tapping Download this test opens a
// separate tabs and loads the URL there. Then closes the tab and waits for
// the download completion.
- (void)testDownloadWhileBrowsing {
  [_helper testDownloadWhileBrowsing];
}

// Tests "Open in New Tab" on download link.
- (void)testDownloadInNewTab {
  [_helper testDownloadInNewTab];
}

// Tests accessibility on Download Manager UI when download is not started.
- (void)testAccessibilityOnNotStartedDownloadToolbar {
  [_helper testAccessibilityOnNotStartedDownloadToolbar];
}

// Tests accessibility on Download Manager UI when download is complete.
- (void)testAccessibilityOnCompletedDownloadToolbar {
  [_helper testAccessibilityOnCompletedDownloadToolbar];
}

// Tests that filename label and "Open in Downloads" button are showing.
- (void)testVisibleFileNameAndOpenInDownloads {
  [_helper testVisibleFileNameAndOpenInDownloads];
}

// Tests that "Open in..." works if the download ended while waiting in a
// different tab which also contains a download task.
- (void)testSwitchTabsAndOpenInDownloads {
  [_helper testSwitchTabsAndOpenInDownloads];
}

// Tests successful blob download. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulBlobDownload {
  [_helper testSuccessfulBlobDownload];
}

// Tests that a pdf can be downloaded. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulPDFDownload {
  [_helper testSuccessfulPDFDownload:NO];
}

// Tests that a file is downloaded successfully even if it is renderable by the
// browser.
- (void)testSuccessfulDownloadWithContentDisposition {
  [_helper testSuccessfulDownloadWithContentDisposition:NO];
}

@end

// Tests for critical user journeys for Download Manager, with Save to Drive.
@interface DownloadManagerWithDriveTestCase : ChromeTestCase
@end

@implementation DownloadManagerWithDriveTestCase {
  DownloadManagerTestCaseHelper* _helper;
}

- (void)setUp {
  [super setUp];
  _helper = [[DownloadManagerTestCaseHelper alloc] init];
  _helper.testServer = self.testServer;
  [_helper setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  configuration.features_enabled.push_back(kIOSSaveToDrive);
  configuration.features_enabled.push_back(kDownloadedPDFOpening);
  return configuration;
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process.
- (void)testSuccessfulDownload {
  [_helper testSuccessfulDownload];
}

// Tests successful download, when the download is triggered in a new page, and
// when finished to "Open" button is displayed.
- (void)testSuccessfulDownloadWithContentDispositionInNewWindow {
  [_helper testSuccessfulDownloadWithContentDispositionInNewWindow];
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. Performs download in Incognito.
#if !TARGET_IPHONE_SIMULATOR
// TODO(crbug.com/40678419): Test consistently failing on device.
#define MAYBE_testSuccessfulDownloadInIncognito \
  DISABLED_testSuccessfulDownloadInIncognito
#else
#define MAYBE_testSuccessfulDownloadInIncognito \
  testSuccessfulDownloadInIncognito
#endif
- (void)MAYBE_testSuccessfulDownloadInIncognito {
  [_helper testSuccessfulDownloadInIncognito];
}

// Tests cancelling download UI.
- (void)testCancellingDownload {
  [_helper testCancellingDownload];
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. After tapping Download this test opens a
// separate tabs and loads the URL there. Then closes the tab and waits for
// the download completion.
- (void)testDownloadWhileBrowsing {
  [_helper testDownloadWhileBrowsing];
}

// Tests "Open in New Tab" on download link.
- (void)testDownloadInNewTab {
  [_helper testDownloadInNewTab];
}

// Tests accessibility on Download Manager UI when download is not started.
- (void)testAccessibilityOnNotStartedDownloadToolbar {
  [_helper testAccessibilityOnNotStartedDownloadToolbar];
}

// Tests accessibility on Download Manager UI when download is complete.
- (void)testAccessibilityOnCompletedDownloadToolbar {
  [_helper testAccessibilityOnCompletedDownloadToolbar];
}

// Tests that filename label and "Open in Downloads" button are showing.
- (void)testVisibleFileNameAndOpenInDownloads {
  [_helper testVisibleFileNameAndOpenInDownloads];
}

// Tests that "Open in..." works if the download ended while waiting in a
// different tab which also contains a download task.
- (void)testSwitchTabsAndOpenInDownloads {
  [_helper testSwitchTabsAndOpenInDownloads];
}

// Tests successful blob download. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulBlobDownload {
  [_helper testSuccessfulBlobDownload];
}

// Tests that a pdf can be downloaded. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulPDFDownload {
  [_helper testSuccessfulPDFDownload:YES];
}

// Tests that a pdf that is displayed in the web view can be downloaded.
// Only valid with "Save to drive" enabled.
// TDOO(crbug.com/343971371): Enable after fixing flakiness.
- (void)FLAKY_testDownloadDisplayedPDF {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/two_pages.pdf")];
  [ChromeEarlGrey waitForPageToFinishLoading];
  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForPageLoadTimeout,
                 ^{
                   NSError* error = nil;
                   [[EarlGrey selectElementWithMatcher:DownloadButton()]
                       assertWithMatcher:grey_interactable()
                                   error:&error];
                   return (error != nil);
                 }),
             @"Download bar did not hide on scroll");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];

  GREYAssert(WaitForDownloadButton(), @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests that a file is downloaded successfully even if it is renderable by the
// browser.
- (void)testSuccessfulDownloadWithContentDisposition {
  [_helper testSuccessfulDownloadWithContentDisposition:YES];
}

@end
