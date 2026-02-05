// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Photos/Photos.h>

#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/download/model/download_app_interface.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/file_upload_panel/ui/constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/buildflags.h"
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
              html += `<p>Name: ${file.name}, ` +
                      `Path: ${file.webkitRelativePath}</p>`;
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
  } else if (request.relative_url == "/accept_text") {
    page_content = GetTestPageHtml("accept=\".txt\"", "", true);
  } else if (request.relative_url == "/accept_video") {
    page_content = GetTestPageHtml("accept=\"video/*\"", "", true);
  } else if (request.relative_url == "/directory") {
    page_content = GetTestPageHtml("webkitdirectory", "", true);
  } else if (request.relative_url == "/single") {
    page_content = GetTestPageHtml("", "Single file input", true);
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

// Loads a test URL with the given `path`, waits for `text` to be visible in
// the web view, and then taps the file input element.
- (void)loadURLAndTapInputWithPath:(const std::string&)path
                       waitForText:(const std::string&)text;

// Opens the file picker and navigates to the downloads directory.
- (void)openFilePickerAndNavigateToDownloads;

// Waits for the SubmittedFileCount histogram to be recorded with the given
// `expectedCount`.
- (void)waitForSubmittedFileCount:(int)expectedCount;

// Waits for `fileCount` files to be present in the downloads directory.
- (void)waitForFileCountInDownloadsDirectory:(int)fileCount;

// Returns the first cell in `app` whose identifier starts with `prefix`.
- (XCUIElement*)cellWithIdentifierPrefix:(NSString*)prefix
                                   inApp:(XCUIElement*)app;

// Returns the "Add" or "Done" button in `app`.
- (XCUIElement*)addOrDoneButtonInApp:(XCUIElement*)app;

@end

@implementation FileUploadPanelTestCase {
  BOOL _isCameraAvailable;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kIOSCustomFileUploadMenu);
  config.features_enabled.push_back(kIOSChooseFromDrive);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&TestPageResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [DownloadAppInterface deleteDownloadsDirectory];
  [self waitForFileCountInDownloadsDirectory:0];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  _isCameraAvailable = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];
  [self checkAndAcceptSystemDialog];
  _isCameraAvailable = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

- (void)checkAndAcceptSystemDialog {
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
}

- (void)loadURLAndTapInputWithPath:(const std::string&)path
                       waitForText:(const std::string&)text {
  GURL url = path.empty() ? self.testServer->base_url()
                          : self.testServer->GetURL(path);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:text];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];
}

// Taps on `element` using coordinates to bypass "isHittable" check.
- (void)forceTap:(XCUIElement*)element {
  GREYAssertNotNil(element, @"Element cannot be nil.\n%@", element);
  XCUICoordinate* coordinate =
      [element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  [coordinate tap];
}

- (void)openFilePickerAndNavigateToDownloads {
  // Interact with the out-of-process file picker.
  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  GREYAssertTrue([serviceApp waitForState:XCUIApplicationStateRunningForeground
                                  timeout:30],
                 @"File picker did not launch");

  if ([ChromeEarlGrey isIPhoneIdiom]) {
    // Tapping Browse twice to navigate to the root.
    XCUIElement* browseButton = serviceApp.buttons[@"Browse"].firstMatch;
    [browseButton tap];
    GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                       base::test::ios::kWaitForActionTimeout,
                       ^{
                         return browseButton.isSelected;
                       }),
                   @"Browse button did not become selected.");
    [browseButton tap];
    XCUIElement* onMyiPhone = serviceApp.cells[@"On My iPhone"].firstMatch;
    GREYAssertTrue([onMyiPhone waitForExistenceWithTimeout:10],
                   @"'On My iPhone' not found.");
    [self forceTap:onMyiPhone];
  } else {
    XCUIElement* onMyiPad =
        serviceApp.cells[@"DOC.sidebar.item.On My iPad"].firstMatch;
    GREYAssertTrue([onMyiPad waitForExistenceWithTimeout:10],
                   @"'On My iPad' not found.");
    [self forceTap:onMyiPad];
  }

  // Once the root directory is visible, select the Downloads folder which is
  // represented by a cell with an ID starting with "ios_chrome_eg2tests".
  XCUIElement* targetCell =
      [self cellWithIdentifierPrefix:@"ios_chrome_eg2tests" inApp:serviceApp];
  [self forceTap:targetCell];
}

// Waits for the SubmittedFileCount histogram to be recorded with the given
// `expectedCount`.
- (void)waitForSubmittedFileCount:(int)expectedCount {
  __block NSError* error = nil;
  BOOL success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        error = [MetricsAppInterface
             expectCount:1
               forBucket:expectedCount
            forHistogram:@"IOS.FileUploadPanel.SubmittedFileCount"];
        return error == nil;
      });
  GREYAssertTrue(
      success,
      @"Histogram IOS.FileUploadPanel.SubmittedFileCount not recorded "
      @"correctly with count %d: %@",
      expectedCount, error);
}

// Waits for `fileCount` files to be present in the downloads directory.
- (void)waitForFileCountInDownloadsDirectory:(int)fileCount {
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForFileOperationTimeout,
          ^{
            return [DownloadAppInterface fileCountInDownloadsDirectory] ==
                   fileCount;
          }),
      @"Timed out waiting for %d files.", fileCount);
}

// Returns the first cell in `app` whose identifier starts with `prefix`.
- (XCUIElement*)cellWithIdentifierPrefix:(NSString*)prefix
                                   inApp:(XCUIElement*)app {
  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:@"identifier BEGINSWITH %@", prefix];
  return [app.cells matchingPredicate:predicate].firstMatch;
}

// Returns the "Add" or "Done" button in `app`.
- (XCUIElement*)addOrDoneButtonInApp:(XCUIElement*)app {
  NSPredicate* addOrDonePredicate =
      [NSPredicate predicateWithFormat:@"label == 'Add' || label == 'Done'"];
  return [[app.buttons matchingPredicate:addOrDonePredicate] firstMatch];
}

// Taps on `cell` until it is selected, with a maximum of 3 taps.
- (void)tapCellUntilSelected:(XCUIElement*)cell inApp:(XCUIApplication*)app {
  GREYAssertTrue([cell waitForExistenceWithTimeout:10], @"'Cell not found:\n%@",
                 cell);
  for (int i = 0; i < 3; i++) {
    [self forceTap:cell];
    // Wait for the cell to become selected.
    BOOL selected =
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10), ^{
          return cell.exists && cell.isSelected;
        });
    if (selected) {
      return;
    }
  }
  GREYAssertTrue(cell.isSelected, @"Cell not selected after 3 taps.");
}

// Taps on `element` until it is not hittable, with a maximum of 3 taps.
- (void)tapElementUntilNotHittable:(XCUIElement*)element
                             inApp:(XCUIApplication*)app {
  GREYAssertTrue([element waitForExistenceWithTimeout:10],
                 @"'Element not found:\n%@", element);
  for (int i = 0; i < 3; i++) {
    [self forceTap:element];
    // Wait for the element to disappear.
    BOOL nonExistent =
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10), ^{
          return !element.exists;
        });
    if (nonExistent) {
      return;
    }
  }
  GREYAssertFalse(element.exists, @"Element still exists after 3 taps.");
}

#if BUILDFLAG(IOS_CHROME_ENABLE_PROFILE_ALTERING_TESTS)

// Adds a video to the photo library.
- (void)addVideoToPhotoLibrary {
  base::FilePath videoPath;
  base::PathService::Get(base::DIR_ASSETS, &videoPath);
  videoPath = videoPath.AppendASCII(
      "ios/testing/data/http_server_files/video_sample.mov");

  NSURL* videoURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(videoPath.value())];

  __block BOOL changesPerformed = NO;
  __block NSError* error = nil;
  [[PHPhotoLibrary sharedPhotoLibrary]
      performChanges:^{
        [PHAssetChangeRequest
            creationRequestForAssetFromVideoAtFileURL:videoURL];
      }
      completionHandler:^(BOOL success, NSError* errorOut) {
        changesPerformed = YES;
        error = errorOut;
      }];

  // Wait for the alert to appear and accept it, or for the changes to complete.
  // The alert might not appear if the permission was already granted.
  BOOL success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        if (changesPerformed) {
          return YES;
        }
        [self checkAndAcceptSystemDialog];
        return changesPerformed;
      });

  GREYAssertTrue(success, @"Failed to add video to photo library: %@", error);
}

// Adds an image to the photo library.
- (void)addImageToPhotoLibrary {
  base::FilePath imagePath;
  base::PathService::Get(base::DIR_ASSETS, &imagePath);
  imagePath = imagePath.AppendASCII(
      "ios/testing/data/http_server_files/chromium_logo.png");

  NSURL* imageURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(imagePath.value())];

  __block BOOL changesPerformed = NO;
  __block NSError* error = nil;
  [[PHPhotoLibrary sharedPhotoLibrary]
      performChanges:^{
        [PHAssetChangeRequest
            creationRequestForAssetFromImageAtFileURL:imageURL];
      }
      completionHandler:^(BOOL success, NSError* errorOut) {
        changesPerformed = YES;
        error = errorOut;
      }];

  // Wait for the alert to appear and accept it, or for the changes to complete.
  // The alert might not appear if the permission was already granted.
  BOOL success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        if (changesPerformed) {
          return YES;
        }
        [self checkAndAcceptSystemDialog];
        return changesPerformed;
      });

  GREYAssertTrue(success, @"Failed to add image to photo library: %@", error);
}

#endif

// Tests that the file upload panel context menu appears and contains expected
// elements when a file input element is tapped.
- (void)testFileUploadPanel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Test entry point and context menu variant histograms.
  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelEntryPointVariant::kContextMenu)
      forHistogram:@"IOS.FileUploadPanel.EntryPointVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  const auto expectedContextMenuVariant =
      _isCameraAvailable
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
       expectCount:_isCameraAvailable ? 1 : 0
         forBucket:static_cast<int>(
                       FileUploadPanelCameraActionVariant::kPhotoAndVideo)
      forHistogram:@"IOS.FileUploadPanel.CameraActionVariant"];
  chrome_test_util::GREYAssertErrorNil(error);

  // Test that expected elements are present.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
              IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL)]
      assertWithMatcher:_isCameraAvailable ? grey_sufficientlyVisible()
                                           : grey_nil()];
}

// Tests that the file upload panel can be dismissed and shown again.
- (void)testDismissAndReshowPanel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)];

  // Tap somewhere else to dismiss the panel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)];

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
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)];

  // Tap somewhere else to dismiss the panel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)];

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

// TODO(crbug.com/458669054): Test is flaky.
// Tests that the camera is presented directly when the capture attribute is
// set.
- (void)FLAKY_testDirectCameraPresentation {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (!_isCameraAvailable) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  [self loadURLAndTapInputWithPath:"/capture_user"
                       waitForText:"File input with capture"];

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
  if (!_isCameraAvailable) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  [self loadURLAndTapInputWithPath:"/accept_image" waitForText:""];

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
  if (!_isCameraAvailable) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  [self loadURLAndTapInputWithPath:"/accept_video" waitForText:""];

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

  [self loadURLAndTapInputWithPath:"/directory" waitForText:""];

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
  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)]
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
  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

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

// Tests that tapping "Google Drive" in the file upload context menu presents
// the Drive file picker.
- (void)testDriveFilePickerFromContextMenu {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Verify "Google Drive" action is present.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CHOOSE_FROM_DRIVE_ACTION_NAME)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Google Drive".
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CHOOSE_FROM_DRIVE_ACTION_NAME)]
      performAction:grey_tap()];

  // Verify Drive File Picker is presented.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kDriveFilePickerAccessibilityIdentifier)];
}

// Tests that tapping the camera action logs the correct metric.
// TODO(crbug.com/459838957): Test is flaky on devices.
#if TARGET_OS_SIMULATOR
#define MAYBE_testContextMenuActionCamera testContextMenuActionCamera
#else
#define MAYBE_testContextMenuActionCamera FLAKY_testContextMenuActionCamera
#endif
- (void)MAYBE_testContextMenuActionCamera {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (!_isCameraAvailable) {
    EARL_GREY_TEST_SKIPPED(@"Camera not available on device, skipping.");
  }

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

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
// TODO(crbug.com/465123880): Failing on device.
- (void)DISABLED_testDirectCameraPresentationAndSubmit {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }
  if (!_isCameraAvailable) {
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
  [self checkAndAcceptSystemDialog];

  // Take photo and check histograms.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"PhotoCapture")]
      performAction:grey_tap()];
  id<GREYMatcher> doneButton =
      grey_allOf(grey_accessibilityID(@"Done"), grey_interactable(), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:doneButton];
  [[EarlGrey selectElementWithMatcher:doneButton] performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:"Name: image.jpg"];

  [self waitForSubmittedFileCount:1];

  NSError* error = nil;
  error = [MetricsAppInterface expectCount:1
                                 forBucket:1  // 1 for success
                              forHistogram:@"IOS.FileUploadPanel.CameraResult"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that cancelling the file picker logs the correct metric.
- (void)testFilePickerCancel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  GURL url = self.testServer->GetURL("/directory");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:""];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Interact with the out-of-process file picker.
  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  GREYAssertTrue([serviceApp waitForState:XCUIApplicationStateRunningForeground
                                  timeout:30],
                 @"File picker did not launch");

  if ([ChromeEarlGrey isIPhoneIdiom]) {
    // Tapping Browse twice to navigate to the root, otherwise the Cancel button
    // will not be visible for the next step.
    XCUIElement* browseButton = serviceApp.buttons[@"Browse"].firstMatch;
    [browseButton tap];
    GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                       base::test::ios::kWaitForActionTimeout,
                       ^{
                         return browseButton.isSelected;
                       }),
                   @"Browse button did not become selected.");
    [browseButton tap];
  }

  [self forceTap:serviceApp.buttons[@"Cancel"].firstMatch];

  // Check histograms.
  [self waitForSubmittedFileCount:0];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:0  // 0 for false (cancelled)
      forHistogram:@"IOS.FileUploadPanel.FilePicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  // The other new histograms should not be recorded.
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"IOS.FileUploadPanel.FilePicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:
              @"IOS.FileUploadPanel.SecurityScopedResource.AccessState"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that picking a directory logs the success metrics.
// TODO(crbug.com/464179603): Marked flaky on simulator because of an iOS bug,
// re-enable test when it has been fixed. The test is also flaky on device.
- (void)FLAKY_testFilePickerDirectorySelectionSuccess {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  // Create a fake file.
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"download.jpg"
                                                     content:@"fake jpeg data"];
  [self waitForFileCountInDownloadsDirectory:1];

  GURL url = self.testServer->GetURL("/directory");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:kFileInputElementID]];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  [self openFilePickerAndNavigateToDownloads];

  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  GREYAssertTrue([serviceApp.buttons[@"Open"] waitForExistenceWithTimeout:10],
                 @"'Open' button not found.");
  [self tapElementUntilNotHittable:serviceApp.buttons[@"Open"].firstMatch
                             inApp:serviceApp];

  // Check that the selected directory is "Documents" which is the real name of
  // the Downloads folder when checking the path.
  [ChromeEarlGrey waitForWebStateContainingText:"Path: Documents"];

  // Check histograms.
  [self waitForSubmittedFileCount:1];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for true (success)
      forHistogram:@"IOS.FileUploadPanel.FilePicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.FilePicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       FileUploadPanelSecurityScopedResourceAccessState::
                           kStartedAndStopped)
      forHistogram:@"IOS.FileUploadPanel.SecurityScopedResource.AccessState"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:
              @"IOS.FileUploadPanel.SecurityScopedResource.AccessState"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that picking a single file logs the success metrics.
// TODO(crbug.com/464179603): Marked flaky on simulator because of an iOS bug,
// re-enable test when it has been fixed.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testFilePickerSingleFileSelectionSuccess \
  FLAKY_testFilePickerSingleFileSelectionSuccess
#else
#define MAYBE_testFilePickerSingleFileSelectionSuccess \
  testFilePickerSingleFileSelectionSuccess
#endif
- (void)MAYBE_testFilePickerSingleFileSelectionSuccess {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  // Create test files.
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"file1.txt"
                                                     content:@"data1"];
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"file2.txt"
                                                     content:@"data2"];
  [self waitForFileCountInDownloadsDirectory:2];

  GURL url = self.testServer->GetURL("/single");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:kFileInputElementID]];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)]
      performAction:grey_tap()];

  [self openFilePickerAndNavigateToDownloads];

  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  // Select the file.
  XCUIElement* fileCell = [self cellWithIdentifierPrefix:@"file1"
                                                   inApp:serviceApp];
  GREYAssertTrue([fileCell waitForExistenceWithTimeout:10],
                 @"'file1.txt' button not hittable.");
  [self tapElementUntilNotHittable:fileCell inApp:serviceApp];

  // Check that the selected file is reported correctly.
  [ChromeEarlGrey waitForWebStateContainingText:"Name: file1.txt"];

  // Check histograms.
  [self waitForSubmittedFileCount:1];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for true (success)
      forHistogram:@"IOS.FileUploadPanel.FilePicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.FilePicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that the `accept` attribute for images correctly disables non-matching
// files.
// TODO(crbug.com/464179603): Marked flaky on simulator because of an iOS bug,
// re-enable test when it has been fixed. The test is also flaky on device.
- (void)FLAKY_testFilePickerAcceptAttributeImage {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  // Create test files.
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"image.jpg"
                                                     content:@"jpeg data"];
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"document.txt"
                                                     content:@"text data"];
  [self waitForFileCountInDownloadsDirectory:2];

  GURL url = self.testServer->GetURL("/accept_image");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:kFileInputElementID]];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)]
      performAction:grey_tap()];

  [self openFilePickerAndNavigateToDownloads];

  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  // Check that the non-image file is disabled and the image file is enabled.
  GREYAssertFalse([[self cellWithIdentifierPrefix:@"document"
                                            inApp:serviceApp] isEnabled],
                  @"document.txt should be disabled");
  XCUIElement* imageCell = [self cellWithIdentifierPrefix:@"image"
                                                    inApp:serviceApp];
  GREYAssertTrue([imageCell isEnabled], @"image.jpg should be enabled");

  // Select the image file.
  [self tapElementUntilNotHittable:imageCell inApp:serviceApp];

  [ChromeEarlGrey waitForWebStateContainingText:"Name: image.jpg"];
}

// Tests that the `accept` attribute for video correctly disables non-matching
// files.
// TODO(crbug.com/464179603): Marked flaky on simulator because of an iOS bug,
// re-enable test when it has been fixed.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testFilePickerAcceptAttributeVideo \
  FLAKY_testFilePickerAcceptAttributeVideo
#else
#define MAYBE_testFilePickerAcceptAttributeVideo \
  testFilePickerAcceptAttributeVideo
#endif
- (void)MAYBE_testFilePickerAcceptAttributeVideo {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  // Create test files.
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"image.jpg"
                                                     content:@"jpeg data"];
  [DownloadAppInterface createDownloadsDirectoryFileWithName:@"video.mov"
                                                     content:@"video data"];
  [self waitForFileCountInDownloadsDirectory:2];

  GURL url = self.testServer->GetURL("/accept_video");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:kFileInputElementID]];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL)]
      performAction:grey_tap()];

  [self openFilePickerAndNavigateToDownloads];

  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  // Check that the non-video file is disabled and the video file is enabled.
  XCUIElement* videoCell = [self cellWithIdentifierPrefix:@"video"
                                                    inApp:serviceApp];
  GREYAssertTrue([videoCell isEnabled], @"video.mov should be enabled");
  GREYAssertFalse([[self cellWithIdentifierPrefix:@"image"
                                            inApp:serviceApp] isEnabled],
                  @"image.jpg should be disabled");

  // Select the video file.
  [self tapElementUntilNotHittable:videoCell inApp:serviceApp];

  // Check that the selected file is reported correctly.
  [ChromeEarlGrey waitForWebStateContainingText:"Name: video.mov"];
}

// Tests that picking multiple files logs the success metrics.
// TODO(crbug.com/464179603): Disabled because of an iOS bug, re-enable test
// when it has been fixed.
- (void)DISABLED_testFilePickerMultipleFileSelectionSuccess {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  // Create test files.
  for (int i = 1; i <= 5; i++) {
    [DownloadAppInterface
        createDownloadsDirectoryFileWithName:[NSString
                                                 stringWithFormat:@"file%d.txt",
                                                                  i]
                                     content:[NSString
                                                 stringWithFormat:@"data%d",
                                                                  i]];
  }
  [self waitForFileCountInDownloadsDirectory:5];

  GURL url = self.testServer->base_url();
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:kFileInputElementID]];

  // Tap the file input to show the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement([ElementSelector
                        selectorWithElementID:kFileInputElementID])];

  // Tap the "Choose File" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL)]
      performAction:grey_tap()];

  [self openFilePickerAndNavigateToDownloads];

  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.DocumentManagerUICore.Service"];
  // Select multiple files.
  for (int i = 1; i <= 5; i++) {
    XCUIElement* fileCell =
        [self cellWithIdentifierPrefix:[NSString stringWithFormat:@"file%d", i]
                                 inApp:serviceApp];
    [self tapCellUntilSelected:fileCell inApp:serviceApp];
  }
  [self tapElementUntilNotHittable:serviceApp.buttons[@"Open"].firstMatch
                             inApp:serviceApp];

  // Check that the selected files are reported correctly.
  for (int i = 1; i <= 5; i++) {
    [ChromeEarlGrey waitForWebStateContainingText:base::StringPrintf(
                                                      "Name: file%d.txt", i)];
  }

  // Check histograms.
  [self waitForSubmittedFileCount:5];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for true (success)
      forHistogram:@"IOS.FileUploadPanel.FilePicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface
       expectCount:1
         forBucket:5
      forHistogram:@"IOS.FileUploadPanel.FilePicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that cancelling the photo picker logs the correct metric.
- (void)testPhotoPickerCancel {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

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

  // Interact with the out-of-process photo picker.
  XCUIApplication* serviceApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.mobileslideshow.photospicker"];
  GREYAssertTrue([serviceApp waitForState:XCUIApplicationStateRunningForeground
                                  timeout:30],
                 @"Photo picker did not launch");

  [self forceTap:serviceApp.buttons[@"Cancel"].firstMatch];

  // Check histograms.
  [self waitForSubmittedFileCount:0];

  error = [MetricsAppInterface
       expectCount:1
         forBucket:0  // 0 for false (cancelled)
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  // The file count histogram should not be recorded.
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"IOS.FileUploadPanel.PhotoPicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

#if BUILDFLAG(IOS_CHROME_ENABLE_PROFILE_ALTERING_TESTS)

// Tests that picking a single photo from the photo picker logs the success
// metrics.
- (void)testPhotoPickerSingleSelection {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Tap the "Photo Library" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
      performAction:grey_tap()];

  XCUIApplication* photosPickerApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.mobileslideshow.photospicker"];
  GREYAssertTrue(
      [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                            timeout:30],
      @"Photo picker did not launch");

  NSPredicate* imagePredicate =
      [NSPredicate predicateWithFormat:@"label BEGINSWITH 'Photo'"];
  XCUIElementQuery* images =
      [photosPickerApp.images matchingPredicate:imagePredicate];

  if (![images.firstMatch waitForExistenceWithTimeout:5]) {
    // Close the picker to add the image.
    [photosPickerApp.buttons[@"Cancel"] tap];

    [self addImageToPhotoLibrary];
    [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

    // Re-open picker.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                       IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
        performAction:grey_tap()];

    GREYAssertTrue(
        [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                              timeout:30],
        @"Photo picker did not launch");

    images = [photosPickerApp.images matchingPredicate:imagePredicate];
  }

  // Select a photo.
  XCUIElement* photo = [[photosPickerApp.images
      matchingIdentifier:@"PXGGridLayout-Info"] elementBoundByIndex:0];
  GREYAssertTrue([photo waitForExistenceWithTimeout:10],
                 @"Photo button not hittable.");
  [self forceTap:photo];
  XCUIElement* addOrDoneButton = [self addOrDoneButtonInApp:photosPickerApp];
  GREYAssertTrue([addOrDoneButton waitForExistenceWithTimeout:10],
                 @"'Add' or 'Done' button not found.");
  [addOrDoneButton tap];

  // Check histograms.
  [self waitForSubmittedFileCount:1];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that picking multiple photos from the photo picker logs the success
// metrics.
- (void)testPhotoPickerMultipleSelection {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Tap the "Photo Library" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
      performAction:grey_tap()];

  XCUIApplication* photosPickerApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.mobileslideshow.photospicker"];
  GREYAssertTrue(
      [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                            timeout:30],
      @"Photo picker did not launch");

  NSPredicate* imagePredicate =
      [NSPredicate predicateWithFormat:@"label BEGINSWITH 'Photo'"];
  XCUIElementQuery* images =
      [photosPickerApp.images matchingPredicate:imagePredicate];

  if (images.count < 2) {
    // Close the picker to add the images.
    [photosPickerApp.buttons[@"Cancel"] tap];

    [self addImageToPhotoLibrary];
    [self addImageToPhotoLibrary];
    [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

    // Re-open picker.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                       IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
        performAction:grey_tap()];

    GREYAssertTrue(
        [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                              timeout:30],
        @"Photo picker did not launch");

    images = [photosPickerApp.images matchingPredicate:imagePredicate];
  }

  // Select multiple photos.
  XCUIElement* photo1 = [images elementBoundByIndex:0];
  XCUIElement* photo2 = [images elementBoundByIndex:1];
  GREYAssertTrue([photo1 waitForExistenceWithTimeout:10],
                 @"Photo 1 button not hittable.");
  [self forceTap:photo1];
  GREYAssertTrue([photo2 waitForExistenceWithTimeout:10],
                 @"Photo 2 button not hittable.");
  [self forceTap:photo2];
  XCUIElement* addOrDoneButton = [self addOrDoneButtonInApp:photosPickerApp];
  GREYAssertTrue([addOrDoneButton waitForExistenceWithTimeout:10],
                 @"'Add' or 'Done' button not found.");
  [addOrDoneButton tap];

  // Check histograms.
  [self waitForSubmittedFileCount:2];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:2
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:2
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Tests that picking a video from the photo picker logs the success metrics.
// TODO(crbug.com/477857213): Flaky on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testPhotoPickerVideoSelection testPhotoPickerVideoSelection
#else
#define MAYBE_testPhotoPickerVideoSelection FLAKY_testPhotoPickerVideoSelection
#endif
- (void)MAYBE_testPhotoPickerVideoSelection {
  // The file upload panel is only available on iOS 18.4+.
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test is only available for iOS 18.4+, skipping.");
  }

  [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

  // Tap the "Photo Library" action.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
      performAction:grey_tap()];

  XCUIApplication* photosPickerApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.mobileslideshow.photospicker"];
  GREYAssertTrue(
      [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                            timeout:30],
      @"Photo picker did not launch");

  NSPredicate* videoPredicate =
      [NSPredicate predicateWithFormat:@"label BEGINSWITH 'Video'"];
  XCUIElementQuery* videos =
      [photosPickerApp.images matchingPredicate:videoPredicate];

  if (![videos.firstMatch waitForExistenceWithTimeout:5]) {
    // Close the picker to add the video.
    [photosPickerApp.buttons[@"Cancel"] tap];

    [self addVideoToPhotoLibrary];
    [self loadURLAndTapInputWithPath:"" waitForText:"File input"];

    // Re-open picker.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                       IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL)]
        performAction:grey_tap()];

    GREYAssertTrue(
        [photosPickerApp waitForState:XCUIApplicationStateRunningForeground
                              timeout:30],
        @"Photo picker did not launch");

    videos = [photosPickerApp.images matchingPredicate:videoPredicate];
  }

  XCUIElement* video = [videos firstMatch];
  GREYAssertTrue([video waitForExistenceWithTimeout:10],
                 @"Video button not hittable.");
  [self forceTap:video];

  XCUIElement* addOrDoneButton = [self addOrDoneButtonInApp:photosPickerApp];
  GREYAssertTrue([addOrDoneButton waitForExistenceWithTimeout:10],
                 @"'Add' or 'Done' button not found.");
  [addOrDoneButton tap];

  // Check histograms.
  [self waitForSubmittedFileCount:1];

  NSError* error = nil;
  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // 1 for success
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result"];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1
      forHistogram:@"IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount"];
  chrome_test_util::GREYAssertErrorNil(error);
}

#endif

@end
