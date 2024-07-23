// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_constants.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/drive/model/test_constants.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/scoped_policy_list.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {

id<GREYMatcher> IdentityButtonMatcherForIdentity(id<SystemIdentity> identity) {
  NSString* accessibility_label = [NSString
      stringWithFormat:@"%@, %@", identity.userFullName, identity.userEmail];
  return grey_allOf(grey_accessibilityID(kIdentityButtonControlIdentifier),
                    grey_accessibilityLabel(accessibility_label), nil);
}

// Matcher for "SAVE..." button on Download Manager UI, which is presented
// instead of the "DOWNLOAD" button when multiple destinations are available for
// downloads.
id<GREYMatcher> SaveEllipsisButton() {
  return grey_accessibilityID(
      kDownloadManagerSaveEllipsisAccessibilityIdentifier);
}

// Matcher for "DOWNLOAD" button when one destination is available for
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
    configuration.additional_args.push_back(base::StringPrintf(
        "--%s=%s", commandLineSwitch.c_str(), commandLineValue.c_str()));
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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveEllipsisButton()];
  [[EarlGrey selectElementWithMatcher:SaveEllipsisButton()]
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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveEllipsisButton()];
  [[EarlGrey selectElementWithMatcher:SaveEllipsisButton()]
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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button. The file
  // name of the file to download is set to
  // `kTestDriveFileUploaderTryAgainFileName` so that the upload fails during
  // the first attempt.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "Drive" button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveEllipsisButton()];
  [[EarlGrey selectElementWithMatcher:SaveEllipsisButton()]
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

// Tests that "DOWNLOAD" button is presented instead of "SAVE..." if signed-out.
- (void)testSignedOutDisablesSaveToDrive {
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "DOWNLOAD" button is presented instead of "SAVE...".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
}

// Tests that "DOWNLOAD" button is presented instead of "SAVE..." in Incognito.
- (void)testIncognitoDisablesSaveToDrive {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Switch to Incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "DOWNLOAD" button is presented instead of "SAVE...".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
}

// Tests that "DOWNLOAD" button is presented instead of "SAVE..." when
// enterprise policy explicitly disables Save to Drive.
- (void)testPolicyDisablesSaveToDrive {
  // Temporary disable Save to Drive using policy.
  ScopedPolicyList disableSaveToDrive;
  disableSaveToDrive.SetPolicy(
      static_cast<int>(SaveToDrivePolicySettings::kDisabled),
      policy::key::kDownloadManagerSaveToDriveSettings);
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "DOWNLOAD" button is presented instead of "SAVE...".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:DownloadButton()];
}

// Tests that when the user taps "Save" in the account selection, the selected
// account and file destination are memorized and presented as the default
// option for the next time.
- (void)testSaveToDriveMemorizesLastSelectedAccount {
  [ChromeEarlGrey clearUserPrefWithName:prefs::kIosSaveToDriveDefaultGaiaId];
  // Sign-in.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  // Add a second fake identity to the device.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  // Load a page with a download button and tap the download button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  // Check that the "SAVE..." button is presented and tap it.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveEllipsisButton()];
  [[EarlGrey selectElementWithMatcher:SaveEllipsisButton()]
      performAction:grey_tap()];
  // Check that the identity button is hidden.
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcherForIdentity(fakeIdentity1)]
      assertWithMatcher:grey_notVisible()];
  // Select "Drive" as destination.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:FileDestinationDriveButton()];
  [[EarlGrey selectElementWithMatcher:FileDestinationDriveButton()]
      performAction:grey_tap()];
  // Check that the selected identity is initially the signed-in identity.
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcherForIdentity(fakeIdentity1)]
      assertWithMatcher:grey_interactable()];
  // Tap the identity button and select the second account.
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcherForIdentity(fakeIdentity1)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::IdentityCellMatcherForEmail(
                                   fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  // Check that the second identity is now selected.
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcherForIdentity(fakeIdentity2)]
      assertWithMatcher:grey_interactable()];
  // Tap "Save".
  [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
      performAction:grey_tap()];
  // Wait for the account picker to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AccountPicker()];
  // Check that after a few seconds, the "GET THE APP" button appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:DownloadManagerGetTheAppButton()
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
  // Tap the download button and the "SAVE..." button again.
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveEllipsisButton()];
  [[EarlGrey selectElementWithMatcher:SaveEllipsisButton()]
      performAction:grey_tap()];
  // Check that the second identity is now selected by default.
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcherForIdentity(fakeIdentity2)]
      assertWithMatcher:grey_interactable()];
}

@end
