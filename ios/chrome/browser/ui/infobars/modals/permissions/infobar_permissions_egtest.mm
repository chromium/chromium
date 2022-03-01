// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "components/strings/grit/components_chromium_strings.h"
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

@interface InfobarPermissionsTestCase : ChromeTestCase
@end

@implementation InfobarPermissionsTestCase

// Tests that accessible permission is shown in Permissions section with toggle.
- (void)testShowAccessibleInfobarPermissions {
  if (@available(iOS 15.0, *)) {
    // Mock the scenario that microphone permission is on while camera
    // permission is not accessible.
    EarlGreyScopedBlockSwizzler microphonePermissionAllowed(
        @"PageInfoPermissionsMediator", @"accessiblePermissionStates", ^{
          return @{@(web::PermissionMicrophone) : @YES};
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
