// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"

#if BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)

namespace {

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

// Matcher for the account picker with only the accessibility identifier.
id<GREYMatcher> AccountPickerIDMatcher() {
  return grey_accessibilityID(
      kAccountPickerScreenNavigationControllerAccessibilityIdentifier);
}

// Matcher for "Save Image in Google Photos" button in context menu.
id<GREYMatcher> SaveImageToPhotosButton() {
  return chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_TOOLS_MENU_SAVE_IMAGE_TO_PHOTOS);
}

// Matcher for "Save image in..." button in context menu.
id<GREYMatcher> SaveImageInButton() {
  return chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_TOOLS_MENU_SAVE_IMAGE_IN);
}

// Provides downloads landing page with an image.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(
      "<br><img id='image' "
      "src='data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGA"
      "h"
      "KmMIQAAAABJRU5ErkJggg==' width='100' height='100'>");
  return result;
}

}  // namespace

@interface SaveToPhotosTestCase : ChromeTestCase
@end

@implementation SaveToPhotosTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&GetResponse)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Only builds with Chrome Branding have multiple options for saving images,
// skip this test in other builds.

// Tests that if the account needs a reauth when saving an image to Photos, and
// the user cancels the reauth, the UI remains usable.
// Regression test for crbug.com/483804450.
- (void)testSaveToPhotosReauthCancel {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setPersistentAuthErrorForAccount:CoreAccountId::FromGaiaId(
                                                       fakeIdentity.gaiaId)];

  // Load a page with an image.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:"image"]];

  // Long press the image to open the context menu.
  [ChromeEarlGreyUI
      longPressElementOnWebView:[ElementSelector
                                    selectorWithElementID:"image"]];

  // Tap "Save image in...".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveImageInButton()];
  [[EarlGrey selectElementWithMatcher:SaveImageInButton()]
      performAction:grey_tap()];

  // Tap "Save in Google Photos".
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SaveImageToPhotosButton()];
  [[EarlGrey selectElementWithMatcher:SaveImageToPhotosButton()]
      performAction:grey_tap()];

  // The account picker should appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];

  // Tap "Save" in the account picker.
  [[EarlGrey selectElementWithMatcher:AccountPickerPrimaryButton()]
      performAction:grey_tap()];

  // Reauth flow should start. Cancel it.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kFakeAuthCancelButtonIdentifier)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

  // The UI should remain usable. We expect the account picker to be still
  // visible (because we were on it) AND we should be able to interact with it
  // (e.g. cancel it).
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AccountPicker()];

  // Try to cancel the account picker to verify it's still responsive.
  id<GREYMatcher> cancelButton = grey_allOf(
      grey_accessibilityID(kAccountPickerCancelButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:cancelButton] performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:AccountPickerIDMatcher()];

  // Verify we can still interact with the main app, e.g. open the tools menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::ToolsMenuView()];
}

// Tests that if the account needs a reauth when saving an image to Photos, and
// the settings are such that the account picker is skipped, and the user
// cancels the reauth, the UI is dismissed and we are back to the tab.
- (void)testSaveToPhotosReauthCancelWithSkipPicker {
  // Sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey setPersistentAuthErrorForAccount:CoreAccountId::FromGaiaId(
                                                       fakeIdentity.gaiaId)];

  // Set prefs to skip account picker and use fakeIdentity1.
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kIosSaveToPhotosSkipAccountPicker];
  [ChromeEarlGrey
      setStringValue:base::SysUTF8ToNSString(fakeIdentity.gaiaId.ToString())
         forUserPref:prefs::kIosSaveToPhotosDefaultGaiaId];

  // Load a page with an image.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:"image"]];

  // Long press the image to open the context menu.
  [ChromeEarlGreyUI
      longPressElementOnWebView:[ElementSelector
                                    selectorWithElementID:"image"]];

  // Tap "Save image in...".
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SaveImageInButton()];
  [[EarlGrey selectElementWithMatcher:SaveImageInButton()]
      performAction:grey_tap()];

  // Tap "Save in Google Photos".
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SaveImageToPhotosButton()];
  [[EarlGrey selectElementWithMatcher:SaveImageToPhotosButton()]
      performAction:grey_tap()];

  // Reauth flow should start directly (skipping account picker). Cancel it.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kFakeAuthCancelButtonIdentifier)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

  // The UI should be dismissed and we should be back to the tab.
  // We check that the account picker is NOT visible and we can interact with
  // the app.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:AccountPickerIDMatcher()];

  // Verify we can still interact with the main app, e.g. open the tools menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::ToolsMenuView()];
}

@end

#endif  // BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
