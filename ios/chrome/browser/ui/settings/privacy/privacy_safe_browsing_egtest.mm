// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Integration tests using the Privacy Safe Browsing settings screen.
@interface PrivacySafeBrowsingTestCase : ChromeTestCase
@end

@implementation PrivacySafeBrowsingTestCase

- (void)testOpenPrivacySafeBrowsingSettings {
}

#pragma mark - Helpers

- (void)openPrivacySafeBrowsingSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  NSString* safeBrowsingDialogLabel =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE);
  [ChromeEarlGreyUI tapPrivacyMenuButton:safeBrowsingDialogLabel];
}

@end
