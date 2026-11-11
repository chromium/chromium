// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/passwords/bottom_sheet/test/remote_actor_credential_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// EarlGrey tests for `RemoteActorCredentialBottomSheetViewController`.
@interface RemoteActorCredentialBottomSheetTestCase : ChromeTestCase
@end

@implementation RemoteActorCredentialBottomSheetTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(
      password_manager::features::kRemoteActorCredentialSharing);
  config.features_disabled.push_back(kEnableNewStartupFlow);
  return config;
}

- (void)tearDownHelper {
  [RemoteActorCredentialBottomSheetAppInterface cleanUpRemoteActorKeychain];
  GREYAssertTrue([PasswordSettingsAppInterface clearPasswordStores],
                 @"Clearing password stores failed.");
  [super tearDownHelper];
}

// Verifies that navigating to a valid Remote Actor URL displays
// `RemoteActorCredentialBottomSheetViewController` when a password is saved
// for the requested domain.
- (void)testBottomSheetShowsForValidRemoteActorURL {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  password_manager_test_utils::SavePasswordFormToAccountStore(
      @"password", @"user@example.com", @"https://example.com/");

  NSString* URLString = [RemoteActorCredentialBottomSheetAppInterface
      createValidRemoteActorURLForDomain:@"example.com"
                                  gaiaID:fakeIdentity.gaiaId.ToNSString()];
  GREYAssertNotNil(URLString, @"Failed to create valid Remote Actor URL.");

  [ChromeEarlGrey sceneOpenURL:GURL(base::SysNSStringToUTF8(URLString))];

  id<GREYMatcher> allowButtonMatcher = grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON),
      grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:allowButtonMatcher];

  id<GREYMatcher> cancelButtonMatcher = grey_allOf(
      grey_accessibilityID(kButtonStackSecondaryActionAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_CANCEL),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:cancelButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the bottom sheet before teardown.
  [[EarlGrey selectElementWithMatcher:cancelButtonMatcher]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:allowButtonMatcher];
}

@end
