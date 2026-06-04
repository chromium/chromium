// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/test/ios/wait_util.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/upload_request_test_server.h"
#import "components/safe_browsing/core/common/safebrowsing_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_app_interface.h"
#import "ios/chrome/browser/download/ui/download_egtest_util.h"
#import "ios/chrome/browser/download/ui/download_manager_constants.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/test/analysis_connectors_app_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForDownloadTimeout;
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

}  // namespace

// Tests for critical user journeys for Download Manager, with Save to Drive.
@interface DownloadManagerTestCase : ChromeTestCase
@end

@implementation DownloadManagerTestCase {
  std::unique_ptr<enterprise_connectors::test::UploadRequestTestServer>
      _uploadServer;
}

- (void)setUp {
  if ([self isEnterpriseDownloadTest]) {
    _uploadServer = std::make_unique<
        enterprise_connectors::test::UploadRequestTestServer>();
    // Start must be called before setUp as the server URL is passed in the
    // APPLaunchConfiguration.
    if (!_uploadServer->Start()) {
      // Use NOTREACHED() instead of GREYAssertTrue because GREYAssertTrue can
      // only be used after calling the -setUp method of the super class.
      NOTREACHED();
    }
  }

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

- (void)tearDownHelper {
  [AnalysisConnectorsAppInterface clearBrowserDMToken];
  [AnalysisConnectorsAppInterface clearDownloadProtectionRules];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  // TODO(crbug.com/6602213): Fix the test suite for when Auto-deletion is
  // enabled.
  configuration.features_disabled.push_back(
      kDownloadAutoDeletionFeatureEnabled);
  configuration.features_disabled.push_back(kIOSSaveToDriveSignedOut);
  configuration.features_disabled.push_back(kChromeNextIa);

  if ([self isEnterpriseDownloadTest]) {
    configuration.features_enabled.push_back(
        enterprise_connectors::kEnableFileDownloadConnectorIOS);
    configuration.additional_args.push_back(base::StrCat(
        {"--", safe_browsing::switches::kCloudBinaryUploadServiceUrlFlag, "=",
         _uploadServer->GetServiceURL().spec()}));
    if ([self isEnterpriseDownloadAllowTest]) {
      _uploadServer->SetScanResultSuccess();
    } else if ([self isEnterpriseDownloadWarnTest]) {
      _uploadServer->SetScanResultWarn();
    } else if ([self isEnterpriseDownloadBlockTest]) {
      _uploadServer->SetScanResultBlock();
    }
  }

  return configuration;
}

- (bool)isEnterpriseDownloadTest {
  return [self isRunningTest:@selector
               (testSuccessfulDownloadWhenDownloadProtectionEnabled)] ||
         [self isRunningTest:@selector
               (testSuccessfulDownloadScanWhenDownloadProtectionEnabled)] ||
         [self isRunningTest:@selector
               (testDownloadBlockWhenDownloadProtectionEnabled)] ||
         [self isRunningTest:@selector
               (testDownloadWarnCancelWhenDownloadProtectionEnabled)] ||
         [self isRunningTest:@selector
               (testDownloadWarnProceedWhenDownloadProtectionEnabled)];
}

- (bool)isEnterpriseDownloadAllowTest {
  return [self isRunningTest:@selector
               (testSuccessfulDownloadScanWhenDownloadProtectionEnabled)];
}

- (bool)isEnterpriseDownloadWarnTest {
  return [self isRunningTest:@selector
               (testDownloadWarnCancelWhenDownloadProtectionEnabled)] ||
         [self isRunningTest:@selector
               (testDownloadWarnProceedWhenDownloadProtectionEnabled)];
}

- (bool)isEnterpriseDownloadBlockTest {
  return [self
      isRunningTest:@selector(testDownloadBlockWhenDownloadProtectionEnabled)];
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
// TODO(crbug.com/515624708): Fails on iPhone devices.
- (void)testVisibleFileNameAndOpenInDownloads {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPhone simulator, crbug.com/438749917 "
                            @"crbug.com/515624708");
  }
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
// TODO(crbug.com/515613762): Fails on iPhone devices.
- (void)testSwitchTabsAndOpenInDownloads {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Failing on iPhone, crbug.com/438749917 crbug.com/515613762");
  }

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
// TODO(crbug.com/502586050): Failing with ChromeNextIA.
- (void)testDownloadDisplayedPDF {
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test is failing with ChromeNextIA enabled. See b/502586050.");
  }
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

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionUp, 150)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:DownloadButton()];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
}

// Tests download can proceed normally for non-enterprise user when Enterprise
// download protection feature is enabled.
- (void)testSuccessfulDownloadWhenDownloadProtectionEnabled {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"DataURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"data"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  GREYAssertEqual(_uploadServer->GetRequestCount(), 0,
                  @"Expected no scan request was made");
}

// Tests download can proceed normally if the scan result is success.
- (void)testSuccessfulDownloadScanWhenDownloadProtectionEnabled {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"DataURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"data"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");
  GREYAssertEqual(_uploadServer->GetRequestCount(), 1,
                  @"Expected 1 scan request but received %d request(s)",
                  _uploadServer->GetRequestCount());
}

// Tests download will be blocked if the scan result is failure, and a snackbar
// message will show.
- (void)testDownloadBlockWhenDownloadProtectionEnabled {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"DataURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"data"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Wait for download to finish and check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_FILE_SAVE_BLOCKED_SNACKBAR_TEXT));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage
                                              timeout:kWaitForDownloadTimeout];

  // Test that the open in and download buttons do not show up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_nil()];

  GREYAssertEqual(_uploadServer->GetRequestCount(), 1,
                  @"Expected 1 scan request but received %d request(s)",
                  _uploadServer->GetRequestCount());
}

// Tests that a warning dialog will show up if the scan result is warn, download
// will be blocked if the user decides to cancel.
- (void)testDownloadWarnCancelWhenDownloadProtectionEnabled {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"DataURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"data"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Wait for download to finish and the dialog to show up.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::AlertItemWithAccessibilityLabelId(IDS_CANCEL)
                                  timeout:kWaitForDownloadTimeout];

  // Tap the "Cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_CANCEL)] performAction:grey_tap()];

  // Test that the open in and download buttons do not show up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_nil()];

  GREYAssertEqual(_uploadServer->GetRequestCount(), 1,
                  @"Expected 1 scan request but received %d request(s)",
                  _uploadServer->GetRequestCount());
}

// Tests that a warning dialog will show up if the scan result is warn, download
// will proceed if the user decides to continue.
// TODO(crbug.com/518764990): Flaky on iPad.
- (void)testDownloadWarnProceedWhenDownloadProtectionEnabled {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Flaky on iPad.");
  }
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/download_test_page.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"DataURL"];
  [ChromeEarlGrey tapWebStateElementWithID:@"data"];

  GREYAssert(WaitForDownloadButton(/*loading*/ true),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Wait for download to finish and the dialog to show up.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::AlertItemWithAccessibilityLabelId(
                          IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON)
                                              timeout:kWaitForDownloadTimeout];

  // Tap the "Continue" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Make sure the open in button show up.
  GREYAssert(WaitForOpenInButton(), @"Open in... button did not show up");

  GREYAssertEqual(_uploadServer->GetRequestCount(), 1,
                  @"Expected 1 scan request but received %d request(s)",
                  _uploadServer->GetRequestCount());
}

@end
