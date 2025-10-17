// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/file_upload_panel/ui/constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// HTML for the test page.
constexpr char kPageHtml[] = R"(

<html>
  <body>
    <h1>File input</h1>
    <input type="file" id="fileInput" />
    <h1>File output</h1>
    <p id="fileContent"></p>
    <script>
        const fileInput = document.getElementById('fileInput');
        const fileContent = document.getElementById('fileContent');
        fileInput.addEventListener('change', (event) => {
          const file = event.target.files[0];
          if (file) {
            const reader = new FileReader();
            reader.onload = (e) => {
              fileContent.textContent = e.target.result;
            };
            reader.onerror = (e) => {
              fileContent.textContent = 'Error reading file.';
              console.error(e);
            };
            reader.readAsText(file); // Assuming text file
          } else {
            fileContent.textContent = 'No file selected.';
          }
        });
    </script>
  </body>
</html>

)";

// Identifier of the file input element.
constexpr char kFileInputElementID[] = "fileInput";

// Returns a test page response with a file upload element.
std::unique_ptr<net::test_server::HttpResponse> TestPageResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(kPageHtml);
  return std::move(http_response);
}

}  // namespace

// Test case for the file upload panel UI.
@interface FileUploadPanelTestCase : ChromeTestCase

@end

@implementation FileUploadPanelTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kIOSCustomFileUploadMenu);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&TestPageResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

// Tests that the file upload panel context menu appears and contains expected
// elements when a file input element is tapped.
- (void)testFileUploadPanel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  GURL url = self.testServer->base_url();
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"File input"];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  const BOOL isCameraAvailable = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];
  const auto expectedContextMenuVariant =
      isCameraAvailable
          ? FileUploadPanelContextMenuVariant::
                kPhotoPickerAndCameraAndFilePicker
          : FileUploadPanelContextMenuVariant::kPhotoPickerAndFilePicker;

  // Test that the file upload panel context menu is the first UI presented for
  // the file upload panel.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(expectedContextMenuVariant)
      forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Test that expected elements are present.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL)]
      assertWithMatcher:isCameraAvailable ? grey_sufficientlyVisible()
                                          : grey_nil()];
}

// Tests that the file upload panel can be dismissed and shown again.
- (void)testDismissAndReshowPanel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  GURL url = self.testServer->base_url();
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"File input"];

  // Tap the file input to show the panel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)];

  // Tap somewhere else to dismiss the panel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)];

  // Tap the file input again to show the panel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)];

  // Check that the metric was recorded twice.
  NSError* error = [MetricsAppInterface
      expectTotalCount:2
          forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

@end
