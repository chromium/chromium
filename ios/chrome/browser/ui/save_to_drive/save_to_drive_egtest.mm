// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/drive/model/test_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_constants.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/download/download_manager_constants.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {

// Matcher for "SAVE..." button on Download Manager UI, which is presented
// instead of the "DOWNLOAD" button when multiple destinations are available for
// downloads.
id<GREYMatcher> DownloadButton() {
  return grey_accessibilityID(kDownloadManagerDownloadAccessibilityIdentifier);
}

// Matcher for "Files" destination button in File destination picker UI.
id<GREYMatcher> FileDestinationFilesButton() {
  return grey_allOf(
      grey_accessibilityID(kFileDestinationPickerFilesAccessibilityIdentifier),
      grey_interactable(), nil);
}

// Matcher for "Drive" destination button in File destination picker UI.
id<GREYMatcher> FileDestinationDriveButton() {
  return grey_allOf(
      grey_accessibilityID(kFileDestinationPickerDriveAccessibilityIdentifier),
      grey_interactable(), nil);
}

// Matcher for "GET THE APP" button on Download Manager UI.
id<GREYMatcher> DownloadManagerGetTheAppButton() {
  return grey_allOf(
      grey_accessibilityID(kDownloadManagerInstallAppAccessibilityIdentifier),
      grey_interactable(), nil);
}

// Matcher for "TRY AGAIN" button on Download Manager UI.
id<GREYMatcher> DownloadManagerTryAgainButton() {
  return grey_allOf(
      grey_accessibilityID(kDownloadManagerTryAgainAccessibilityIdentifier),
      grey_interactable(), nil);
}

// Matcher for the account picker.
id<GREYMatcher> AccountPicker() {
  return grey_allOf(
      grey_accessibilityID(
          kAccountPickerScreenNavigationControllerAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the "Save" button in the account picker.
id<GREYMatcher> AccountPickerPrimaryButton() {
  return grey_allOf(
      grey_accessibilityID(kAccountPickerPrimaryButtonAccessibilityIdentifier),
      grey_interactable(), nil);
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

}  // namespace

@interface SaveToDriveTestCase : ChromeTestCase
@end

@implementation SaveToDriveTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&GetResponse)));

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/download-example",
      base::BindRepeating(&testing::HandleDownload)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration configuration;
  configuration.features_enabled.push_back(kIOSSaveToDrive);
  if ([self isRunningTest:@selector(testCanRetryDownloadToDrive)]) {
    const std::string commandLineSwitch =
        std::string(kTestDriveFileUploaderCommandLineSwitch);
    const std::string commandLineValue =
        std::string(kTestDriveFileUploaderCommandLineSwitchFailAndThenSucceed);
    configuration.additional_args.push_back(
        std::format("--{}={}", commandLineSwitch, commandLineValue));
  }
  return configuration;
}

// Tests that when the user is signed-in, they can choose "Files" as destination
// for their download in the file destination picker, tap "Save" in the account
// picker. Tests that after a few seconds, the file has been downloaded
// successfully and a "OPEN IN..." button is displayed.
- (void)testCanDownloadToFiles {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  // Wait for the account picker to appear, select "Files" and tap "Save".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:FileDestinationFilesButton()];
  [[EarlGrey selectElementWithMatcher:FileDestinationFilesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
      performAction:grey_tap()];
  // Wait for the account picker to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AccountPicker()];
  // Check that after a few seconds, the "OPEN IN..." button appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OpenInButton()
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
}

// Tests that when the user is signed-in, they can choose "Drive" as destination
// for their download in the file destination picker, tap "Save" in the account
// picker. Tests that after a few seconds, the file has been downloaded
// successfully and a "GET THE APP" button is displayed.
- (void)testCanDownloadToDrive {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  // Wait for the account picker to appear, select "Drive" and tap "Save".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:FileDestinationDriveButton()];
  [[EarlGrey selectElementWithMatcher:FileDestinationDriveButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
      performAction:grey_tap()];
  // Wait for the account picker to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AccountPicker()];
  // Check that after a few seconds, the "GET THE APP" button appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DownloadManagerGetTheAppButton()
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
}

// Tests that when the user is signed-in, they can choose Drive as destination
// for their download, tap "Save" in the account picker. Tests that after a few
// seconds, if the file upload fails, the user can tap "TRY AGAIN..." in the
// download manager, and after a few seconds, when the upload succeeds, an "GET
// THE APP" button is displayed.
- (void)testCanRetryDownloadToDrive {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button. The file
  // name of the file to download is set to
  // `kTestDriveFileUploaderTryAgainFileName` so that the upload fails during
  // the first attempt.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];
  // Wait for the account picker to appear, select "Drive" and tap "Save".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:FileDestinationDriveButton()];
  [[EarlGrey selectElementWithMatcher:FileDestinationDriveButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
      performAction:grey_tap()];
  // Wait for the account picker to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AccountPicker()];
  // Check that after a few seconds, when the uplaod fails, the "Try Again..."
  // button appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DownloadManagerTryAgainButton()
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
  [[EarlGrey selectElementWithMatcher:DownloadManagerTryAgainButton()]
      performAction:grey_tap()];
  // Check that after a few seconds, when the upload succeeds, the "GET THE APP"
  // button appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DownloadManagerGetTheAppButton()
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
}

@end
