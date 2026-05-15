// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/upload_request_test_server.h"
#import "components/safe_browsing/core/common/safebrowsing_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/test/analysis_connectors_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

NSString* const kPNGFilename = @"chromium_logo";

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

// Path which leads to a PNG file.
const char kPNGPath[] = "/chromium_logo.png";

// Path which leads to a MOV file.
const char kMOVPath[] = "/video_sample.mov";

}  // namespace

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Tests Open in Feature.
@interface ShareFileDownloadTestCase : ChromeTestCase
@end

@implementation ShareFileDownloadTestCase {
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
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDownHelper {
  [AnalysisConnectorsAppInterface clearBrowserDMToken];
  [AnalysisConnectorsAppInterface clearDownloadProtectionRules];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isEnterpriseDownloadTest]) {
    config.features_enabled.push_back(
        enterprise_connectors::kEnableFileDownloadConnectorIOS);
    config.additional_args.push_back(base::StrCat(
        {"--", safe_browsing::switches::kCloudBinaryUploadServiceUrlFlag, "=",
         _uploadServer->GetServiceURL().spec()}));
  }
  if ([self isEnterpriseDownloadAllowTest]) {
    _uploadServer->SetScanResultSuccess();
  } else if ([self isEnterpriseDownloadWarnTest]) {
    _uploadServer->SetScanResultWarn();
  } else if ([self isEnterpriseDownloadBlockTest]) {
    _uploadServer->SetScanResultBlock();
  }
  return config;
}

- (bool)isEnterpriseDownloadTest {
  return
      [self isRunningTest:@selector(testEnterpriseDLPEnabledOpenInPDF)] ||
      [self isRunningTest:@selector
            (testEnterpriseDLPEnabledOpenInPDFScanSucceed)] ||
      [self isRunningTest:@selector(testEnterpriseDLPEnabledOpenInPDFBlock)] ||
      [self isRunningTest:@selector
            (testEnterpriseDLPEnabledOpenInPDFWarnProceed)] ||
      [self
          isRunningTest:@selector(testEnterpriseDLPEnabledOpenInPDFWarnCancel)];
}

- (bool)isEnterpriseDownloadAllowTest {
  return [self
      isRunningTest:@selector(testEnterpriseDLPEnabledOpenInPDFScanSucceed)];
}

- (bool)isEnterpriseDownloadWarnTest {
  return [self isRunningTest:@selector
               (testEnterpriseDLPEnabledOpenInPDFWarnProceed)] ||
         [self isRunningTest:@selector
               (testEnterpriseDLPEnabledOpenInPDFWarnCancel)];
}

- (bool)isEnterpriseDownloadBlockTest {
  return [self isRunningTest:@selector(testEnterpriseDLPEnabledOpenInPDFBlock)];
}

#pragma mark - Public

- (void)openActivityMenu {
  [ChromeEarlGreyUI openShareMenu];
}

#pragma mark - Tests

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenInPDF {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that when Enterprise DLP feature is enabled and user is non-Enterprise
// user, tapping the share button will open the activity sheet normally.
- (void)testEnterpriseDLPEnabledOpenInPDF {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that for Enterprise user, when the scan result for the file is
// successful, the activity sheet will open normally.
- (void)testEnterpriseDLPEnabledOpenInPDFScanSucceed {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that for Enterprise user, when the scan result for the file is block,
// the activity sheet will not open and a snackbar message will show.
- (void)testEnterpriseDLPEnabledOpenInPDFBlock {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  // Wait for download to finish and check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_FILE_SHARE_BLOCKED_SNACKBAR_TEXT));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage
                                              timeout:kWaitForDownloadTimeout];

  // Check that the activity sheet will not open.
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that for Enterprise user, when the scan result for the file is warn and
// user chooses to proceed anyway, the activity sheet will open normally.
- (void)testEnterpriseDLPEnabledOpenInPDFWarnProceed {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

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

  // Test that activity sheet can be open normally.
  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that for Enterprise user, when the scan result for the file is warn and
// user chooses to cancel, the activity sheet will not open.
- (void)testEnterpriseDLPEnabledOpenInPDFWarnCancel {
  [AnalysisConnectorsAppInterface setDownloadProtectionRules];
  [AnalysisConnectorsAppInterface setBrowserDMToken];

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  // Wait for download to finish and the dialog to show up.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::AlertItemWithAccessibilityLabelId(IDS_CANCEL)
                                  timeout:kWaitForDownloadTimeout];

  // Tap the "Cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_CANCEL)] performAction:grey_tap()];

  // Check that the activity sheet will not open.
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that open in button appears when opening a PNG, and that tapping on it
// will open the activity view.
- (void)testOpenInPNG {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  [self openActivityMenu];
  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that open in button do not appears when opening a MOV file.
- (void)testOpenInMOV {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kMOVPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that open in button appears when opening a PNG and when shutting down
// the test server, the appropriate error message is displayed.
- (void)testOpenInOfflineServer {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  // Shutdown the test server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shutdown.");

  // Open the activity menu.
  [self openActivityMenu];
  [ChromeEarlGrey verifyActivitySheetVisible];
  // Ensure that the link is shared.
  [ChromeEarlGrey verifyTextNotVisibleInActivitySheetWithID:kPNGFilename];
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

@end
