// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "components/strings/grit/components_chromium_strings.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/permissions/permissions_app_interface.h"
#import "ios/chrome/browser/ui/permissions/permissions_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher infobar modal camera permissions switch.
id<GREYMatcher> CameraPermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kPageInfoCameraSwitchAccessibilityIdentifier, isOn);
}

// Matcher infobar modal microphone permissions switch.
id<GREYMatcher> MicrophonePermissionsSwitch(BOOL isOn) {
  return chrome_test_util::TableViewSwitchCell(
      kPageInfoMicrophoneSwitchAccessibilityIdentifier, isOn);
}
}  // namespace

@interface PageInfoTestCase : ChromeTestCase
@end

@implementation PageInfoTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if (@available(iOS 15.0, *)) {
    config.features_enabled.push_back(web::features::kMediaPermissionsControl);
  }
  return config;
}

// Checks that if the alert for site permissions pops up, and allow it.
- (void)checkAndAllowPermissionAlerts {
  XCUIApplication* app = [[XCUIApplication alloc] init];
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
  // Allow site permission.
  XCUIElement* alert =
      [[app descendantsMatchingType:XCUIElementTypeAlert] firstMatch];
  XCUIElement* button = alert.buttons[@"Allow"];
  GREYAssertNotNil(button, @"Cannot find \"Allow\" button in system alert.");
  [button tap];
}

// Checks `expectedStatesForPermissions` matches the actual states for
// permissions of the active web state; checks will fail if there is no active
// web state.
- (void)checkStatesForPermissions:
    (NSDictionary<NSNumber*, NSNumber*>*)expectedStatesForPermissions
    API_AVAILABLE(ios(15.0)) {
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

// Tests that rotating the device will don't dismiss the page info view.
- (void)testShowPageInfoRotation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI openPageInfo];

  // Checks that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotates the device and checks that the page info view is still presented.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Closes the page info using the 'Done' button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that opening the page info on a Chromium page displays the correct
// information.
- (void)testShowPageInfoChromePage {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGreyUI openPageInfo];

  // Checks that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Checks that “You’re viewing a secure Chrome page.” is displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_INTERNAL_PAGE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Permissions section is not displayed, as there isn't any
// accessible permissions.
- (void)testShowPageInfoWithNoAccessiblePermission {
  if (@available(iOS 15.0, *)) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
    [ChromeEarlGreyUI openPageInfo];
    // Checks that permission header is not visible.
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                     IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER))]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that single accessible permission is shown in Permissions section with
// toggle.
// TODO(crbug.com/1316705): Test fails on device due to asking for microphone
// permission.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowOneAccessiblePermissionInPageInfo \
  DISABLED_testShowOneAccessiblePermissionInPageInfo
#else
#define MAYBE_testShowOneAccessiblePermissionInPageInfo \
  testShowOneAccessiblePermissionInPageInfo
#endif
- (void)MAYBE_testShowOneAccessiblePermissionInPageInfo {
  if (@available(iOS 15.0, *)) {
    // Open a page that requests microphone permissions.
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey
        loadURL:self.testServer->GetURL("/permissions/microphone_only.html")];
    [self checkAndAllowPermissionAlerts];

    // Check that permission header is visible.
    [ChromeEarlGreyUI openPageInfo];
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                     IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER))]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Check that camera permission item is hidden, and in accordance with the
    // web state permission states.
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
      @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
    }];
    [[EarlGrey
        selectElementWithMatcher:grey_anyOf(CameraPermissionsSwitch(YES),
                                            CameraPermissionsSwitch(NO), nil)]
        assertWithMatcher:grey_notVisible()];
    // Check that microphone permission item is visible, and turn it off.
    [[EarlGrey selectElementWithMatcher:MicrophonePermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    [[EarlGrey  // Dismiss view.
        selectElementWithMatcher:grey_accessibilityID(
                                     kPageInfoViewAccessibilityIdentifier)]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateNotAccessible),
      @(web::PermissionMicrophone) : @(web::PermissionStateBlocked)
    }];
  }
}

// Tests that two accessible permissions are shown in Permissions section with
// toggle.
// TODO(crbug.com/1316705): Test fails on device due to asking for microphone
// permission.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowTwoAccessiblePermissionsInPageInfo \
  DISABLED_testShowTwoAccessiblePermissionsInPageInfo
#else
#define MAYBE_testShowTwoAccessiblePermissionsInPageInfo \
  testShowTwoAccessiblePermissionsInPageInfo
#endif
- (void)MAYBE_testShowTwoAccessiblePermissionsInPageInfo {
  if (@available(iOS 15.0, *)) {
    // Open a page that requests microphone permissions.
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey loadURL:self.testServer->GetURL(
                                "/permissions/camera_and_microphone.html")];
    [self checkAndAllowPermissionAlerts];

    // Check that permission header is visible.
    [ChromeEarlGreyUI openPageInfo];
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                     IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER))]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Check that switchs for both permissions are visible.
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateAllowed),
      @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
    }];
    // Check that both permission item is visible, and turn off camera
    // permission.
    [[EarlGrey selectElementWithMatcher:MicrophonePermissionsSwitch(YES)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:CameraPermissionsSwitch(YES)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
    [[EarlGrey  // Dismiss view.
        selectElementWithMatcher:grey_accessibilityID(
                                     kPageInfoViewAccessibilityIdentifier)]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
    [self checkStatesForPermissions:@{
      @(web::PermissionCamera) : @(web::PermissionStateBlocked),
      @(web::PermissionMicrophone) : @(web::PermissionStateAllowed)
    }];
  }
}

@end
