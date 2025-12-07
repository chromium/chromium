// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_app_interface.h"
#import "ios/chrome/browser/download/ui/download_egtest_util.h"
#import "ios/chrome/browser/download/ui/download_manager_constants.h"
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
using download::DownloadButton;
using download::WaitForDownloadButton;
using download::WaitForOpenInButton;
using download::WaitForOpenPDFButton;

namespace {

// Accessibility ID of the Activity menu.
NSString* const kActivityMenuIdentifier = @"ActivityListView";

// Scroll to the top of the Reading List.
void ScrollToTop() {
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  // The center of the status bar is in the notch. Aim at the bottom left.
  [[springboardApplication.statusBars.firstMatch
      coordinateWithNormalizedOffset:CGVectorMake(0.05, 0.95)] tap];
}

}  // namespace

// Tests for critical user journeys for Download Manager, with Save to Drive.
@interface DownloadManagerTestCase : ChromeTestCase
@end

@implementation DownloadManagerTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&download::GetResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/link-to-content-disposition",
      base::BindRepeating(&download::GetLinkToContentDispositionResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/content-disposition",
      base::BindRepeating(&download::GetContentDispositionPDFResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/download-example",
      base::BindRepeating(&testing::HandleDownload)));

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("ios/testing/data/http_server_files/"));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  // TODO(crbug.com/6602213): Fix the test suite for when Auto-deletion is
  // enabled.
  configuration.features_disabled.push_back(
      kDownloadAutoDeletionFeatureEnabled);
  return configuration;
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process.
- (void)testSuccessfulDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests successful download, after tapping the download button in the web page
// twice, up to the point where "Open in..." button is presented. EarlGrey does
// not allow testing "Open in..." dialog, because it is run in a separate
// process.
- (void)testSuccessfulDownloadAfterTappingPageDownloadButtonTwice {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kDownloadManagerDownloadReplacingOverlayAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_OK)] performAction:grey_tap()];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests successful download up to the point where "Open in..." button is
// presented. EarlGrey does not allow testing "Open in..." dialog, because it
// is run in a separate process. Performs download in Incognito.
#if !TARGET_OS_SIMULATOR
// TODO(crbug.com/40678419): Test consistently failing on device.
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

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests cancelling download UI.
- (void)testCancellingDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
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

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
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
  GREYAssert(WaitForDownloadButton(/*loading*/ false),
             @"Download button did not show up");

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

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on Download Manager UI when download is complete.
// TODO(crbug.com/438749917): Flaky on iPhone simulators.
- (void)testAccessibilityOnCompletedDownloadToolbar {
#if TARGET_IPHONE_SIMULATOR
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPhone simulator, crbug.com/438749917");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that filename label and "Open in Downloads" button are showing.
// TODO(crbug.com/438749917): Flaky on iPhone simulators.
- (void)testVisibleFileNameAndOpenInDownloads {
#if TARGET_IPHONE_SIMULATOR
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPhone simulator, crbug.com/438749917");
  }
#endif
  // Apple is hiding UIActivityViewController's contents from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      verifyTextVisibleInActivitySheetWithID:l10n_util::GetNSString(
                                                 IDS_IOS_OPEN_IN_DOWNLOADS)];

  // Scroll up to find the text if necessary. Verify that the filename label is
  // visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kActivityMenuIdentifier)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 100)
      onElementWithMatcher:grey_text(@"download-example")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that "Open in..." works if the download ended while waiting in a
// different tab which also contains a download task.
// TODO(crbug.com/438749917): Flaky on iPhone simulators.
- (void)testSwitchTabsAndOpenInDownloads {
#if TARGET_IPHONE_SIMULATOR
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPhone simulator, crbug.com/438749917");
  }
#endif

  // Apple is hiding UIActivityViewController's contents from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }

  // Clear the already downloaded file in downloads directory.
  [DownloadAppInterface
      deleteDownloadsDirectoryFileWithName:@"download-example"];
  base::test::ios::SpinRunLoopWithMinDelay(
      base::test::ios::kWaitForFileOperationTimeout);

  // Create a download A task in one tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");

  // Go to a second tab and start a download B.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
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

  // Scroll up to find the text if necessary. Verify that the filename label is
  // visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kActivityMenuIdentifier)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 100)
      onElementWithMatcher:grey_text(@"download-example")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests successful blob download. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download attribute.
- (void)testSuccessfulBlobDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"BlobURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"blob"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests that a pdf can be downloaded. This also checks that a file can be
// downloaded and saved locally while an anchor tag has the download
// attribute.The `shouldOpen` used to wait for the right button once the
// download button is tapped.
- (void)testSuccessfulPDFDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
}

// Tests that a file is downloaded successfully even if it is renderable by the
// browser.
- (void)testSuccessfulDownloadWithContentDisposition {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/link-to-content-disposition")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
}

// Tests that a file is downloaded successfully when opened in a new window.
- (void)testSuccessfulDownloadWithContentDispositionInNewWindow {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/link-to-content-disposition")];
  [ChromeEarlGrey waitForWebStateContainingText:"PDF"];
  [ChromeEarlGrey tapWebStateElementWithID:@"pdf_new_window"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenPDFButton(), @"Open button did not show up");
}

// Tests that a pdf that is displayed in the web view can be downloaded.
// Only valid with "Save to drive" enabled.
- (void)testDownloadDisplayedPDF {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/two_pages.pdf")];
  [ChromeEarlGrey waitForPageToFinishLoading];
  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  // On iOS17, the PDF is not loaded when the bar appear, and saddly, there is
  // no event that happens when the PDF is actually ready. Add a timer as a
  // best effort.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(3));
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

  ScrollToTop();
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:DownloadButton()];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

@end
