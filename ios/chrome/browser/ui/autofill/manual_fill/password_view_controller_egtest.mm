// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackManagePasswordsMatcher;
using chrome_test_util::ManualFallbackManageSettingsMatcher;
using chrome_test_util::ManualFallbackOtherPasswordsDismissMatcher;
using chrome_test_util::ManualFallbackOtherPasswordsMatcher;
using chrome_test_util::ManualFallbackPasswordButtonMatcher;
using chrome_test_util::ManualFallbackPasswordIconMatcher;
using chrome_test_util::ManualFallbackPasswordSearchBarMatcher;
using chrome_test_util::ManualFallbackPasswordTableViewMatcher;
using chrome_test_util::ManualFallbackPasswordTableViewWindowMatcher;
using chrome_test_util::ManualFallbackSuggestPasswordMatcher;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsPasswordMatcher;
using chrome_test_util::SettingsPasswordSearchMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::TapWebElementWithIdInFrame;
using chrome_test_util::UseSuggestedPasswordMatcher;

namespace {

const char kFormElementUsername[] = "username";
const char kFormElementPassword[] = "password";

const char kExampleUsername[] = "concrete username";

const char kFormHTMLFile[] = "/username_password_field_form.html";
const char kIFrameHTMLFile[] = "/iframe_form.html";

// Returns a matcher for the example username in the list.
id<GREYMatcher> UsernameButtonMatcher() {
  return grey_buttonTitle(base::SysUTF8ToNSString(kExampleUsername));
}

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Matcher for the confirmation dialog Continue button.
id<GREYMatcher> ConfirmUsingOtherPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE),
                    grey_interactable(), nullptr);
}

// Matcher for the confirmation dialog Cancel button.
id<GREYMatcher> CancelUsingOtherPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_CANCEL),
                    grey_interactable(), nullptr);
}

}  // namespace

// Integration Tests for Mannual Fallback Passwords View Controller.
@interface PasswordViewControllerTestCase : ChromeTestCase

// URL of the current page.
@property(assign) GURL URL;

@end

@implementation PasswordViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  self.URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:self.URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
  [AutofillAppInterface saveExamplePasswordForm];
}

- (void)tearDown {
  [AutofillAppInterface clearPasswordStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordUISplit);

  return config;
}

// Tests that the passwords view controller appears on screen.
- (void)testPasswordsViewControllerIsPresented {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the passwords view controller contains the "Manage Passwords..."
// and "Manage Settings..." actions.
- (void)testPasswordsViewControllerContainsManageActions {
  // TODO(crbug.com/1352059): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller contains the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
      assertWithMatcher:grey_interactable()];

  // Verify the password controller contains the "Manage Settings..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageSettingsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Manage Passwords..." action works.
- (void)testManagePasswordsActionOpensPasswordSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the "Manage Settings..." action works.
- (void)testManageSettingsActionOpensPasswordSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the "Manage Passwords..." action works in incognito mode.
- (void)testManagePasswordsActionOpensPasswordSettingsInIncognito {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  self.URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:self.URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the "Manage Settings..." action works in incognito mode.
- (void)testManageSettingsActionOpensPasswordSettingsInIncognito {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  self.URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:self.URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Settings..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that returning from "Manage Settings..." leaves the keyboard and the
// icons in the right state.
- (void)testPasswordsStateAfterPresentingManageSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the password view.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Use Other Password..." action works.
- (void)testUseOtherPasswordActionOpens {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  std::u16string origin = base::ASCIIToUTF16(
      password_manager::GetShownOrigin(url::Origin::Create(self.URL)));

  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_MESSAGE, origin);

  [[EarlGrey selectElementWithMatcher:grey_text(message)]
      assertWithMatcher:grey_notNil()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:ConfirmUsingOtherPasswordButton()]
      performAction:grey_tap()];

  // Verify the use other passwords opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Use Other Password..." screen won't open if canceled.
- (void)testUseOtherPasswordActionCloses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Cancel using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:CancelUsingOtherPasswordButton()]
      performAction:grey_tap()];

  // Verify the use other passwords not opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that returning from "Use Other Password..." leaves the view and icons
// in the right state.
- (void)testPasswordsStateAfterPresentingUseOtherPassword {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:ConfirmUsingOtherPasswordButton()]
      performAction:grey_tap()];

  // Verify the use other passwords opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the password view.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is still present after tapping the
// search bar.
- (void)testPasswordControllerWhileSearching {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Select Password..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:ConfirmUsingOtherPasswordButton()]
      performAction:grey_tap()];

  // Verify the use other passwords opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordSearchBarMatcher()]
      performAction:grey_tap()];

  // Verify keyboard is shown and that the password controller is still present
  // in the background.
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.5)];
}

// Tests that the Password View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissPasswordController {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The keyboard icon is never present in iPads.
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissPasswordController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackPasswordTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the password controller table view is not visible and the password
  // icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_interactable()];
  // Verify the interaction status of the password icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests that the Password View Controller is dismissed when tapping the
// keyboard.
// TODO(crbug.com/909629): started to be flaky and sometimes opens full list
// when typing text.
- (void)DISABLED_testTappingKeyboardDismissPasswordControllerPopOver {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      performAction:grey_replaceText(@"text")];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller stays on rotation.
- (void)testPasswordControllerSupportsRotation {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  // Verify the password controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that content is injected in iframe messaging.
- (void)testPasswordControllerSupportsIFrameMessaging {
  const GURL URL = self.testServer->GetURL(kIFrameHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"iFrame"];
  NSString* URLString = base::SysUTF8ToNSString(URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithIdInFrame(kFormElementUsername, 0)];

  // Wait for the accessory icon to appear.
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:
          @"window.frames[0].document.getElementById('%s').value === '%s'",
          kFormElementUsername, kExampleUsername];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

// Tests that an alert is shown when trying to fill a password in an unsecure
// field.
- (void)testPasswordControllerPresentsUnsecureAlert {
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  // Only Objc objects can cross the EDO portal.
  NSString* URLString = base::SysUTF8ToNSString(URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a password.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordButtonMatcher()]
      performAction:grey_tap()];

  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];

  // Dismiss the alert.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];
}

// Tests that the password icon is not present when no passwords are available.
- (void)testPasswordIconIsNotVisibleWhenPasswordStoreEmpty {
  [AutofillAppInterface clearPasswordStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Assert the password icon is not enabled and not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests password generation on manual fallback.
// TODO(crbug.com/1394448): enable the test with fix.
- (void)DISABLED_testPasswordGenerationOnManualFallback {
  // Disable the test on iOS 15.3 due to build failure.
  // TODO(crbug.com/1304685): enable the test with fix.
  if (@available(iOS 15.3, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 15.3.");
  }
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a 'Suggest Password...' option.
  [[EarlGrey selectElementWithMatcher:ManualFallbackSuggestPasswordMatcher()]
      performAction:grey_tap()];

  // Confirm by tapping on the 'Use Suggested Password' button.
  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordMatcher()]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kFormElementPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

@end
