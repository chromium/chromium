// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/permissions/permissions_app_interface.h"
#import "ios/chrome/browser/ui/permissions/permissions_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/common/features.h"
#include "ios/web/public/permissions/permissions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for banner shown when camera permission is enabled.
id<GREYMatcher> InfobarBannerCameraOnly() {
  return grey_allOf(grey_accessibilityID(kInfobarBannerViewIdentifier),
                    grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_ACCESSIBLE)),
                    nil);
}

// Matcher for banner shown when microphone permission is enabled.
id<GREYMatcher> InfobarBannerMicrophoneOnly() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerViewIdentifier),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_BANNER_MICROPHONE_ACCESSIBLE)),
      nil);
}

// Matcher for camera infobar badge with acceptance state |accepted|;
id<GREYMatcher> CameraBadge(BOOL accepted) {
  NSString* axid =
      accepted ? kBadgeButtonPermissionsCameraAcceptedAccessibilityIdentifier
               : kBadgeButtonPermissionsCameraAccessibilityIdentifier;
  return grey_accessibilityID(axid);
}

// Matcher for microphone infobar badge with acceptance state |accepted|;
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
#if !TARGET_OS_MACCATALYST
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if (@available(iOS 15.0, *)) {
    config.features_enabled.push_back(web::features::kMediaPermissionsControl);
  }
  return config;
}

#pragma mark - Helper functions

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

// Checks that if the alert for site permissions pops up, and deny it.
- (void)checkAndDenyPermissionAlerts {
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
  XCUIElement* button = alert.buttons[@"Don’t Allow"];
  GREYAssertNotNil(button,
                   @"Cannot find \"Don't Allow\" button in system alert.");
  [button tap];
}

// Checks that the visibility of the infobar matches |shouldShow|.
- (void)waitUntilInfobarBannerVisibleOrTimeout:(BOOL)shouldShow {
  GREYCondition* infobarShown = [GREYCondition
      conditionWithName:@"Infobar shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_accessibilityID(kInfobarBannerViewIdentifier)]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for infobar to be shown or timeout after 1 second.
  BOOL success = [infobarShown waitWithTimeout:1];
  if (shouldShow) {
    GREYAssertTrue(success, @"Infobar does not appear.");
  } else {
    GREYAssertFalse(success,
                    @"Infobar appears despite that no permission is allowed.");
  }
}

// Checks |expectedStatesForPermissions| matches the actual states for
// permissions of the active web state; checks will fail if there is no active
// web state.
- (void)checkStatesForPermissions:
    (NSDictionary<NSNumber*, NSNumber*>*)expectedStatesForPermissions
    API_AVAILABLE(ios(15.0)) {
  NSDictionary<NSNumber*, NSNumber*>* actualStatesForPermissions =
      [PermissionsAppInterface statesForAllPermissions];
  GREYAssertEqualObjects(expectedStatesForPermissions[@(web::PermissionCamera)],
                         actualStatesForPermissions[@(web::PermissionCamera)],
                         @"Camera state does not match expected.");
  GREYAssertEqualObjects(
      expectedStatesForPermissions[@(web::PermissionMicrophone)],
      actualStatesForPermissions[@(web::PermissionMicrophone)],
      @"Microphone state does not match expected.");
}

#pragma mark - Test cases

// Tests that when camera permission is granted, the user could see a banner
// notification and then toggle camera permission through the infobar modal
// through pressing the "Edit" button on the banner.
- (void)testAllowAndBlockCameraPermission {
  if (@available(iOS 15.0, *)) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey
        loadURL:self.testServer->GetURL("/permissions/camera_only.html")];
    [self checkAndAllowPermissionAlerts];
    [self waitUntilInfobarBannerVisibleOrTimeout:YES];
    [[EarlGrey selectElementWithMatcher:InfobarBannerCameraOnly()]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Tap on the "Edit" button.
    [[[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kInfobarBannerAcceptButtonIdentifier)]
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
  if (@available(iOS 15.0, *)) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey
        loadURL:self.testServer->GetURL("/permissions/microphone_only.html")];
    [self checkAndAllowPermissionAlerts];
    [self waitUntilInfobarBannerVisibleOrTimeout:YES];
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
}

// Tests that when permissions are denied, there will not be a banner or badge.
- (void)testDenyPermissions {
  if (@available(iOS 15.0, *)) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey loadURL:self.testServer->GetURL(
                                "/permissions/camera_and_microphone.html")];
    [self checkAndDenyPermissionAlerts];
    [self waitUntilInfobarBannerVisibleOrTimeout:NO];
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

// TODO(crbug.com/1311069): Addes more tests including overflow and incognito
// behaviors.
#endif
@end
