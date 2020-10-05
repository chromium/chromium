// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::CancelButton;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackPasswordIconMatcher;
using chrome_test_util::ManualFallbackPasswordTableViewMatcher;
using chrome_test_util::ManualFallbackPasswordSearchBarMatcher;
using chrome_test_util::ManualFallbackManagePasswordsMatcher;
using chrome_test_util::ManualFallbackOtherPasswordsMatcher;
using chrome_test_util::ManualFallbackOtherPasswordsDismissMatcher;
using chrome_test_util::ManualFallbackPasswordButtonMatcher;
using chrome_test_util::ManualFallbackPasswordTableViewWindowMatcher;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::SettingsPasswordMatcher;
using chrome_test_util::SettingsPasswordSearchMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::TapWebElementWithIdInFrame;

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

// Polls the JavaScript query |java_script_condition| until the returned
// |boolValue| is YES with a kWaitForActionTimeout timeout.
BOOL WaitForJavaScriptCondition(NSString* java_script_condition) {
  auto verify_block = ^BOOL {
    id value = [ChromeEarlGrey executeJavaScript:java_script_condition];
    return [value isEqual:@YES];
  };
  NSTimeInterval timeout = base::test::ios::kWaitForActionTimeout;
  NSString* condition_name = [NSString
      stringWithFormat:@"Wait for JS condition: %@", java_script_condition];
  GREYCondition* condition =
      [GREYCondition conditionWithName:condition_name block:verify_block];
  return [condition waitWithTimeout:timeout];
}

}  // namespace

// Integration Tests for Mannual Fallback Passwords View Controller.
@interface PasswordViewControllerTestCase : ChromeTestCase
@end

@implementation PasswordViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
  [AutofillAppInterface saveExamplePasswordForm];
}

- (void)tearDown {
  [AutofillAppInterface clearPasswordStore];
  [super tearDown];
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
// action.
- (void)testPasswordsViewControllerContainsManagePasswordsAction {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller contains the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
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
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Passwords..." leaves the keyboard and the
// icons in the right state.
- (void)testPasswordsStateAfterPresentingManagePasswords {
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
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
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

  // Verify the use other passwords opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
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

  // Verify the use other passwords opened.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
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

// Tests that the Password View Controller is not present when presenting UI.
- (void)testPasswordControllerPauses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:SettingsPasswordSearchMatcher()]
      performAction:grey_tap()];

  // Verify keyboard is shown without the password controller.
  GREYAssertTrue([ChromeEarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is resumed after selecting other
// password.
// TODO(crbug.com/981922): Re-enable this test due to failing DB call.
- (void)DISABLED_testPasswordControllerResumes {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Other Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordSearchBarMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the password list to disappear. Using the search bar, since the
  // popover doesn't have it.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordSearchBarMatcher()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Password View Controller is resumed after dismissing "Other
// Passwords".
// TODO(crbug.com/984977): Support this behavior again.
- (void)DISABLED_testPasswordControllerResumesWhenOtherPasswordsDismiss {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Other Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Dismiss the Other Passwords view.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackOtherPasswordsDismissMatcher()]
      performAction:grey_tap()];

  // Wait for the password list to disappear. Using the search bar, since the
  // popover doesn't have it.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordSearchBarMatcher()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
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
      performAction:grey_typeText(@"text")];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that after switching fields the content size of the table view didn't
// grow.
- (void)testPasswordControllerKeepsRightSize {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the "Manage Passwords..." is on screen.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the second element.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the "Manage Passwords..." is on screen.
  [[EarlGrey selectElementWithMatcher:ManualFallbackOtherPasswordsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
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

  [ChromeEarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
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
  GREYAssertTrue([ChromeEarlGrey isKeyboardShownWithError:nil],
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
  XCTAssertTrue(WaitForJavaScriptCondition(javaScriptCondition));
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
  GREYAssertTrue([ChromeEarlGrey isKeyboardShownWithError:nil],
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
}

// Tests that the password icon is not present when no passwords are available.
- (void)testPasswordIconIsNotVisibleWhenPasswordStoreEmpty {
  [AutofillAppInterface clearPasswordStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the keyboard to appear.
  GREYAssertTrue([ChromeEarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Assert the password icon is not enabled and not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

@end
