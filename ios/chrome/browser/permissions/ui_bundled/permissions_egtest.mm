// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_app_interface.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/permissions/permissions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

// Matcher for banner shown when camera permission is enabled.
id<GREYMatcher> InfobarBannerCameraOnly() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_ACCESSIBLE)),
      nil);
}

// Matcher for banner shown when microphone permission is enabled.
id<GREYMatcher> InfobarBannerMicrophoneOnly() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_BANNER_MICROPHONE_ACCESSIBLE)),
      nil);
}

// Matcher for banner shown when both camera and microphone permissions are
// enabled.
id<GREYMatcher> InfobarBannerCameraAndMicrophone() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_AND_MICROPHONE_ACCESSIBLE)),
      nil);
}

// Matcher for infobar banner "Edit" button.
id<GREYMatcher> InfobarBannerEditButton() {
  return grey_accessibilityID(kInfobarBannerAcceptButtonIdentifier);
}

// Matcher for camera infobar badge with acceptance state `accepted`;
id<GREYMatcher> CameraBadge(BOOL accepted) {
  NSString* axid =
      accepted ? kBadgeButtonPermissionsCameraAcceptedAccessibilityIdentifier
               : kBadgeButtonPermissionsCameraAccessibilityIdentifier;
  return grey_accessibilityID(axid);
}

// Matcher for microphone infobar badge with acceptance state `accepted`;
id<GREYMatcher> MicrophoneBadge(BOOL accepted) {
  NSString* axid =
      accepted
          ? kBadgeButtonPermissionsMicrophoneAcceptedAccessibilityIdentifier
          : kBadgeButtonPermissionsMicrophoneAccessibilityIdentifier;
  return grey_accessibilityID(axid);
}

// Matcher infobar modal camera permissions switch.
id<GREYMatcher> InfobarModalCameraPermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kInfobarModalCameraSwitchAccessibilityIdentifier, isOn);
}

// Matcher infobar modal microphone permissions switch.
id<GREYMatcher> InfobarModalMicrophonePermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kInfobarModalMicrophoneSwitchAccessibilityIdentifier, isOn);
}

// Taps "Done" button to dismiss modal when shown.
void TapDoneButtonOnInfobarModal() {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kInfobarModalCancelButton)]
      performAction:grey_tap()];
}

}  // namespace

// This test tests the infobar banner, modal and badge behaviors related to
// permissions.
@interface PermissionsTestCase : ChromeTestCase
@end

@implementation PermissionsTestCase

#pragma mark - Helper functions

// Checks that if the alert for site permissions pops up with
// `permissionsString` that shows permissions requested, and allow or deny it.
- (void)checkAndTapAlertContainingPermissions:(NSString*)permissionsString
                                  shouldAllow:(BOOL)allow {
  //  Allow system permission if shown.
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
  // Click button on site permissions dialog.
  id<GREYMatcher> dialogMatcher =
      grey_accessibilityID(kPermissionsDialogAccessibilityIdentifier);
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:dialogMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Permissions dialog was not shown.");
  NSString* alertText = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE,
      base::UTF8ToUTF16(self.testServer->base_url().host()),
      base::SysNSStringToUTF16(permissionsString));
  id<GREYMatcher> textMatcher = grey_allOf(
      grey_ancestor(dialogMatcher), grey_accessibilityLabel(alertText), nil);
  [[EarlGrey selectElementWithMatcher:textMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* buttonText = l10n_util::GetNSString(
      allow ? IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT
            : IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY);

  id<GREYMatcher> buttonMatcher = grey_allOf(
      grey_ancestor(dialogMatcher), grey_accessibilityLabel(buttonText),
      grey_accessibilityTrait(UIAccessibilityTraitStaticText), nil);

  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];
}

// Checks `expectedStatesForPermissions` matches the actual states for
// permissions of the active web state; checks will fail if there is no active
// web state.
- (void)checkStatesForPermissions:
    (NSDictionary<NSNumber*, NSNumber*>*)expectedStatesForPermissions {
  NSDictionary<NSNumber*, NSNumber*>* actualStatesForPermissions =
      [PermissionsAppInterface statesForAllPermissions];
  GREYAssertEqualObjects(
      expectedStatesForPermissions[@(web::PermissionCamera)],
      actualStatesForPermissions[@(web::PermissionCamera)],
      @"Camera state: %@ does not match expected: %@.",
      actualStatesForPermissions[@(web::PermissionCamera)],
      expectedStatesForPermissions[@(web::PermissionCamera)]);
  GREYAssertEqualObjects(
      expectedStatesForPermissions[@(web::PermissionMicrophone)],
      actualStatesForPermissions[@(web::PermissionMicrophone)],
      @"Microphone state: %@ does not match expected: %@.",
      actualStatesForPermissions[@(web::PermissionMicrophone)],
      expectedStatesForPermissions[@(web::PermissionMicrophone)]);
}

#pragma mark - Test cases

// Tests that when camera permission is granted, the user could see a banner
// notification and then toggle camera permission through the infobar modal
// through pressing the "Edit" button on the banner.
- (void)testAllowAndBlockCameraPermission {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 17.5.");
  }
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/camera_only.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self checkAndTapAlertContainingPermissions:
              l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                    shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraOnly()]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Tap "Edit" to show infobar modal.
    [[[EarlGrey selectElementWithMatcher:InfobarBannerEditButton()]
        performAction:grey_tap()] assertWithMatcher:grey_notVisible()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
    [[EarlGrey
        selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
    TapDoneButtonOnInfobarModal();
    [[EarlGrey selectElementWithMatcher:CameraBadge(/*accepted=*/NO)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Tests that when microphone permission is granted, the user could see a banner
// notification and then toggle microphone permission through the infobar modal
// through the location bar badge.
- (void)testAllowAndBlockMicrophonePermission {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/microphone_only.html")];
  [self checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_MICROPHONE)
                                  shouldAllow:YES];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
  [[EarlGrey selectElementWithMatcher:InfobarBannerMicrophoneOnly()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Swipe up to dismiss.
  [[[EarlGrey selectElementWithMatcher:InfobarBannerMicrophoneOnly()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)]
      assertWithMatcher:grey_notVisible()];
  // Tap badges button to show modal.
  [[EarlGrey selectElementWithMatcher:MicrophoneBadge(/*accepted=*/YES)]
      performAction:grey_tap()];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
  }];
  [[EarlGrey
      selectElementWithMatcher:InfobarModalMicrophonePermissionsSwitch(YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateBlocked)
  }];
  TapDoneButtonOnInfobarModal();
  [[EarlGrey selectElementWithMatcher:MicrophoneBadge(/*accepted=*/NO)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when both camera and microphone permissions are granted, the user
// could see a banner notification and then toggle the permissions through both
// the infobar modal and the location bar badge.
- (void)testAllowAndBlockCameraAndMicrophonePermissions {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 17.5.");
  }
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/permissions/camera_and_microphone.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self
        checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA_AND_MICROPHONE)
                                  shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraAndMicrophone()]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Tap "Edit" to show infobar modal.
    [[[EarlGrey selectElementWithMatcher:InfobarBannerEditButton()]
        performAction:grey_tap()] assertWithMatcher:grey_notVisible()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
    }];
    // Block camera permission.
    [[EarlGrey
        selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    // Taps "Done" button. Check infobar badge and tap it.
    TapDoneButtonOnInfobarModal();
    [[EarlGrey selectElementWithMatcher:MicrophoneBadge(/*accepted=*/YES)]
        performAction:grey_tap()];
    // Check current permission states and block microphone permission as
    // well.
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
    }];
    [[EarlGrey
        selectElementWithMatcher:InfobarModalMicrophonePermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    // Taps "Done" button. Check infobar badge and tap it again.
    TapDoneButtonOnInfobarModal();
    [[EarlGrey selectElementWithMatcher:CameraBadge(/*accepted=*/NO)]
        performAction:grey_tap()];
    // Check current permission states and re-enable camera permission.
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateBlocked)
    }];
    [[EarlGrey selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(NO)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
    TapDoneButtonOnInfobarModal();
    [[EarlGrey selectElementWithMatcher:CameraBadge(/*accepted=*/YES)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateBlocked)
    }];
  }
}

// Tests that when permissions are denied, there will not be a banner or badge.
- (void)testDenyPermissions {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/permissions/camera_and_microphone.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self
        checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA_AND_MICROPHONE)
                                  shouldAllow:NO];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
    id<GREYMatcher> anyPermissionBadge =
        grey_anyOf(CameraBadge(/*accepted=*/YES), CameraBadge(NO),
                   MicrophoneBadge(YES), MicrophoneBadge(NO), nil);
    [[EarlGrey selectElementWithMatcher:anyPermissionBadge]
        assertWithMatcher:grey_nil()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
  }
}

// Tests that permissions infobar banner, modal and badge works the same way in
// incognito by toggling camera access.
- (void)testTogglePermissionsInIncognito {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 17.5.");
  }
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/camera_only.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self checkAndTapAlertContainingPermissions:
              l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                    shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraOnly()]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Tap "Edit" to show infobar modal.
    [[[EarlGrey selectElementWithMatcher:InfobarBannerEditButton()]
        performAction:grey_tap()] assertWithMatcher:grey_notVisible()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
    [[EarlGrey
        selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
    TapDoneButtonOnInfobarModal();
    [[EarlGrey selectElementWithMatcher:CameraBadge(/*accepted=*/NO)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Tests that permissions infobar banner is correctly dismissed when the
// incognito browser is killed.
- (void)testDismissPermissionsWhenIncognitoKilled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/camera_only.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self checkAndTapAlertContainingPermissions:
              l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                    shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

    // Kill the incognito browser while the banner is still presented.
    [ChromeEarlGreyUI openTabGrid];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
}

// Tests that permissions are reset after user navigation.
- (void)testPermissionsAfterNavigation {
  // TODO(crbug.com/40921852): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Failing on iOS17");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/microphone_only.html")];
  [self checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_MICROPHONE)
                                  shouldAllow:YES];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
  [[EarlGrey selectElementWithMatcher:InfobarBannerEditButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:InfobarModalMicrophonePermissionsSwitch(YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  TapDoneButtonOnInfobarModal();
  [[EarlGrey selectElementWithMatcher:MicrophoneBadge(/*accepted=*/NO)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate away and go back.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey goBack];

  // Note: There's currently an existing WebKit bug that WKUIDelegate method
  // `requestMediaCapturePermissionForOrigin:` would not be invoked when the
  // user hits backward/forward; therefore, the alert and banner would not
  // show again, and the checks for the alert and the infobar banner are
  // commented out. Once this issue is fixed, these checks should be
  // uncommented.

  /*[self checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_MICROPHONE)
                                  shouldAllow:YES];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];*/
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
  }];
}

// Tests that permissions stay the same after user switches to another tab then
// comes back.
- (void)testPermissionsAfterTabSwitch {
  // TODO(crbug.com/40921852): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Failing on iOS17");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Opens a page that requests both camera and microphone permissions, and
  // block microphone permission.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/permissions/camera_and_microphone.html")];
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self
        checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA_AND_MICROPHONE)
                                  shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerEditButton()]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  }
  TapDoneButtonOnInfobarModal();
  [[EarlGrey selectElementWithMatcher:MicrophoneBadge(/*accepted=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Switches tab and go back.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey selectTabAtIndex:0];
  // Check if permission states stay the same. Wrap this in a GREYCondition
  // since it takes a small timeout for the web state to retrieve its state.
  GREYCondition* microphoneAcceptedBadgeShown = [GREYCondition
      conditionWithName:@"Microphone accepted badge visible"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:MicrophoneBadge(
                                                            /*accepted=*/YES)]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for infobar to be shown or timeout after kWaitForUIElementTimeout.
  BOOL success = [microphoneAcceptedBadgeShown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(success, @"Did not find accepted microphone badge.");
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateBlocked),
    @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
  }];
}

// Tests that permissions are reset after reload.
- (void)testPermissionsAfterReload {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 17.5.");
  }
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Opens a page that requests camera permission.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/permissions/camera_only.html")];

  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self checkAndTapAlertContainingPermissions:
              l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                    shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraOnly()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Reload and allow again and check if things work as expected. Wait until
  // permissions are reset to validate behaviors.
  [ChromeEarlGrey reload];
  GREYCondition* permissionReset = [GREYCondition
      conditionWithName:@"Permission is reset"
                  block:^BOOL {
                    NSDictionary<NSNumber*, NSNumber*>*
                        actualStatesForPermissions =
                            [PermissionsAppInterface statesForAllPermissions];
                    return [actualStatesForPermissions[@(web::PermissionCamera)]
                        isEqualToNumber:@(web::PermissionStateNotAccessible)];
                  }];
  BOOL success = [permissionReset
      waitWithTimeout:base::test::ios::kWaitForPageLoadTimeout.InSecondsF()];
  GREYAssertTrue(success,
                 @"Camera permission state is not reset after reload.");
  {
    // It is suspected that the video element in the test page performs some
    // fast repetitive animations that, combined with the EarlGrey
    // synchronization, delays the invocation of the infobar appearance
    // animation completion block. As a workaround, we disables EarlGrey
    // synchronization whenever the test is showing the video element.
    ScopedSynchronizationDisabler disabler;
    [self checkAndTapAlertContainingPermissions:
              l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                    shouldAllow:YES];
    [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraOnly()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey selectElementWithMatcher:CameraBadge(/*accepted=*/YES)]
        performAction:grey_tap()];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
    [[EarlGrey
        selectElementWithMatcher:InfobarModalCameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    TapDoneButtonOnInfobarModal();
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
    }];
  }

  // Reload and deny to check if permissions are no longer accessible.
  [ChromeEarlGrey reload];
  [self checkAndTapAlertContainingPermissions:
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA)
                                  shouldAllow:NO];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
  id<GREYMatcher> anyPermissionBadge =
      grey_anyOf(CameraBadge(/*accepted=*/YES), CameraBadge(NO),
                 MicrophoneBadge(YES), MicrophoneBadge(NO), nil);
  [[EarlGrey selectElementWithMatcher:anyPermissionBadge]
      assertWithMatcher:grey_nil()];
}

// Tests that a supervised user account with parental controls enabled does not
// have access to modify camera or microphone site permissions.
- (void)testSupervisedUserPermissionsNoCameraOrMicAccess {
  // TODO(crbug.com/40921852): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Failing on iOS17");
  }

  // These settings are controlled in Family Link and would be updated through
  // Sync content settings.
  [ChromeEarlGrey setContentSetting:ContentSetting::CONTENT_SETTING_BLOCK
             forContentSettingsType:ContentSettingsType::MEDIASTREAM_CAMERA];
  [ChromeEarlGrey setContentSetting:ContentSetting::CONTENT_SETTING_BLOCK
             forContentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/permissions/camera_and_microphone.html")];

  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
  [self checkStatesForPermissions:@{
    @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
    @(web::PermissionMicrophone) : @(web::PermissionStateNotAccessible)
  }];
}

@end
