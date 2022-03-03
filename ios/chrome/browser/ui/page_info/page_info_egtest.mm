// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "components/strings/grit/components_chromium_strings.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Tests that accessible permission is shown in Permissions section with toggle.
- (void)testShowAccessiblePermissionInPageInfo {
  if (@available(iOS 15.0, *)) {
    // Mock the scenario that microphone permission is on while camera
    // permission is not accessible.

    PermissionInfo* permissionDescription = [[PermissionInfo alloc] init];
    permissionDescription.permission = web::PermissionMicrophone;
    permissionDescription.state = web::PermissionStateAllowed;

    EarlGreyScopedBlockSwizzler microphonePermissionAllowed(
        @"PageInfoViewController", @"permissionsInfo", ^{
          return @[ permissionDescription ];
        });

    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
    [ChromeEarlGreyUI openPageInfo];

    // Check that permission header is visible.
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                     IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER))]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Check that camera permission item is hidden.
    [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                            IDS_IOS_PERMISSIONS_CAMERA))]
        assertWithMatcher:grey_notVisible()];
    // Check that microphone permission item is visible and enabled.
    [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                            IDS_IOS_PERMISSIONS_MICROPHONE))]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:grey_switchWithOnState(YES)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

@end
