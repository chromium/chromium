// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
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

// Identifier of the file input element.
constexpr char kFileInputElementID[] = "fileInput";

// Returns an HTML page with a file input element.
std::string GetTestPageHtml(const std::string& input_element_attributes,
                            const std::string& page_title,
                            bool with_script) {
  std::string script_html = R"(
    <h1>File output</h1>
    <div id="fileOutput"></div>
    <script>
        const fileInput = document.getElementById('fileInput');
        const fileOutput = document.getElementById('fileOutput');
        fileInput.addEventListener('change', (event) => {
          debugger;
          const files = event.target.files;
          let html = '';
          if (files.length > 0) {
            for (const file of files) {
              html += `<p>Name: ${file.name}, Size: ${file.size}, Type: ${file.type}</p>`;
            }
          } else {
            html = 'No file selected.';
          }
          fileOutput.innerHTML = html;
        });
    </script>
  )";

  std::string h1_html =
      page_title.empty()
          ? ""
          : base::StringPrintf("<h1>%s</h1>", page_title.c_str());

  return base::StringPrintf(R"(
<html>
  <body>
    %s
    <input type="file" id="fileInput" %s>
    %s
  </body>
</html>
      )",
                            h1_html.c_str(), input_element_attributes.c_str(),
                            with_script ? script_html.c_str() : "");
}

// Returns a test page response with a file upload element.
std::unique_ptr<net::test_server::HttpResponse> TestPageResponse(
    const net::test_server::HttpRequest& request) {
  std::string page_content;
  if (request.relative_url == "/capture_user") {
    page_content =
        GetTestPageHtml("capture=\"user\"", "File input with capture", true);
  } else if (request.relative_url == "/accept_image") {
    page_content = GetTestPageHtml("accept=\"image/*\"", "", true);
  } else if (request.relative_url == "/accept_video") {
    page_content = GetTestPageHtml("accept=\"video/*\"", "", true);
  } else if (request.relative_url == "/directory") {
    page_content = GetTestPageHtml("webkitdirectory", "", true);
  } else {
    page_content = GetTestPageHtml("multiple", "File input", true);
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(page_content);
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

  // Test entry point and context menu variant histograms.
  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelEntryPointVariant::kContextMenu)
      forHistogram:@"IOS.FileUploadPanel.EntryPointVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  const BOOL isCameraAvailable = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];
  const auto expectedContextMenuVariant =
      isCameraAvailable
          ? FileUploadPanelContextMenuVariant::
                kPhotoPickerAndCameraAndFilePicker
          : FileUploadPanelContextMenuVariant::kPhotoPickerAndFilePicker;

  // Test that the file upload panel context menu is the first UI presented for
  // the file upload panel.
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(expectedContextMenuVariant)
      forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Test camera action variant histogram.
  error = [MetricsAppInterface
       expectCount:isCameraAvailable ? 1 : 0
         forBucket:static_cast<int>(
                       FileUploadPanelCameraActionVariant::kPhotoAndVideo)
      forHistogram:@"IOS.FileUploadPanel.CameraActionVariant"];
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

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:0
      forHistogram:@"IOS.FileUploadPanel.SubmittedFileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Tap the file input again to show the panel.
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

  // Check that the metric was recorded twice.
  error = [MetricsAppInterface
      expectTotalCount:2
          forHistogram:@"IOS.FileUploadPanel.ContextMenuVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:2
         forBucket:0
      forHistogram:@"IOS.FileUploadPanel.SubmittedFileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that the camera is presented directly when the capture attribute is
// set.
- (void)testDirectCameraPresentation {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  GURL url = self.testServer->GetURL("/capture_user");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"File input with capture"];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Verify camera is shown by waiting for the camera view controller container.
  id<GREYMatcher> matcher =
      grey_kindOfClassName(@"CAMCameraViewControllerContainerView");
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:matcher];

  // Verify that the context menu is not shown.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)]
      assertWithMatcher:grey_nil()];

  // Test entry point histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(FileUploadPanelEntryPointVariant::kCamera)
      forHistogram:@"IOS.FileUploadPanel.EntryPointVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Tap cancel and check camera result histogram.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"DismissImagePickerButton")]
      performAction:grey_tap()];

  error = [MetricsAppInterface expectCount:1
                                 forBucket:0
                              forHistogram:@"IOS.FileUploadPanel.CameraResult"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:0
      forHistogram:@"IOS.FileUploadPanel.SubmittedFileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that the camera action label is "Take Photo" when only images are
// accepted.
- (void)testCameraActionLabelTakePhoto {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  GURL url = self.testServer->GetURL("/accept_image");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:""];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Verify the label.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_ACTION_LABEL)];
  // And verify the other labels are not present.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_TAKE_VIDEO_ACTION_LABEL)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL)]
      assertWithMatcher:grey_nil()];

  // Test camera action variant histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(FileUploadPanelCameraActionVariant::kPhoto)
      forHistogram:@"IOS.FileUploadPanel.CameraActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that the camera action label is "Take Video" when only videos are
// accepted.
- (void)testCameraActionLabelTakeVideo {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  GURL url = self.testServer->GetURL("/accept_video");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:""];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Verify the label.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_TAKE_VIDEO_ACTION_LABEL)];
  // And verify the other labels are not present.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_ACTION_LABEL)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL)]
      assertWithMatcher:grey_nil()];

  // Test camera action variant histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(FileUploadPanelCameraActionVariant::kVideo)
      forHistogram:@"IOS.FileUploadPanel.CameraActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that the file picker is presented directly when the directory attribute
// is set.
- (void)testEntryPointFilePicker {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  GURL url = self.testServer->GetURL("/directory");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:""];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Test entry point histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelEntryPointVariant::kFilePicker)
      forHistogram:@"IOS.FileUploadPanel.EntryPointVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that tapping the "Choose File" action logs the correct metric.
- (void)testContextMenuActionFilePicker {
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

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)]
      performAction:grey_tap()];

  // Test context menu action variant histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelContextMenuActionVariant::kFilePicker)
      forHistogram:@"IOS.FileUploadPanel.ContextMenuActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that tapping the "Photo Library" action logs the correct metric.
- (void)testContextMenuActionPhotoPicker {
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

  // Tap the "Photo Library" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
      performAction:grey_tap()];

  // Test context menu action variant histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelContextMenuActionVariant::kPhotoPicker)
      forHistogram:@"IOS.FileUploadPanel.ContextMenuActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that tapping the camera action logs the correct metric.
- (void)testContextMenuActionCamera {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  GURL url = self.testServer->base_url();
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"File input"];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Tap the camera action.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL)]
      performAction:grey_tap()];

  // Test context menu action variant histogram.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelContextMenuActionVariant::kCamera)
      forHistogram:@"IOS.FileUploadPanel.ContextMenuActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Verify camera is shown.
  id<GREYMatcher> matcher =
      grey_kindOfClassName(@"CAMCameraViewControllerContainerView");
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:matcher];
}

// Tests that taking a photo and submitting it records the correct metrics. Test
// is disabled on simulator because the camera cannot be used.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testDirectCameraPresentationAndSubmit \
  DISABLED_testDirectCameraPresentationAndSubmit
#else
#define MAYBE_testDirectCameraPresentationAndSubmit \
  testDirectCameraPresentationAndSubmit
#endif
- (void)MAYBE_testDirectCameraPresentationAndSubmit {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  GURL url = self.testServer->GetURL("/capture_user");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"File input with capture"];

  // Tap the file input.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Verify camera is shown by waiting for the camera view controller container.
  id<GREYMatcher> matcher =
      grey_kindOfClassName(@"CAMCameraViewControllerContainerView");
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:matcher];

  // Allow system permission if shown.
  NSError* systemAlertFoundError = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&systemAlertFoundError];
  if (systemAlertFoundError) {
    NSError* acceptAlertError = nil;
    [self grey_acceptSystemDialogWithError:&acceptAlertError];
    GREYAssertNil(acceptAlertError, @"Error accepting system alert.\n%@",
                  acceptAlertError);
  }

  // Take photo and check histograms.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"PhotoCapture")]
      performAction:grey_tap()];
  id<GREYMatcher> doneButton =
      grey_allOf(grey_accessibilityID(@"Done"), grey_interactable(), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:doneButton];
  [[EarlGrey selectElementWithMatcher:doneButton] performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:"Name: image.jpg"];

  NSError* error =
      [MetricsAppInterface expectCount:1
                             forBucket:1  // 1 for success
                          forHistogram:@"IOS.FileUploadPanel.CameraResult"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.SubmittedFileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

@end
