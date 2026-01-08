// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface SyncEncryptionTableViewTestCase : ChromeTestCase
@end

@implementation SyncEncryptionTableViewTestCase

// Tests to open the sync passphrase settings twice.
- (void)testSyncPassphraseSettingsTwice {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  id<GREYMatcher> elementMatcher =
      grey_allOf(grey_accessibilityID(kEncryptionAccessibilityIdentifier),
                 grey_interactable(), nil);
  id<GREYAction> searchScrollAction =
      grey_scrollInDirection(kGREYDirectionDown, 100.0f);
  [[[[EarlGrey selectElementWithMatcher:elementMatcher]
      inRoot:scrollViewMatcher] usingSearchAction:searchScrollAction
                             onElementWithMatcher:scrollViewMatcher]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSyncFullEncryptionAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuBackButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSyncFullEncryptionAccessibilityIdentifier)]
      performAction:grey_tap()];
}

@end
