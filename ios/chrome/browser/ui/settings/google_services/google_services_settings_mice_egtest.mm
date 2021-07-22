// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#include "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::BookmarkHomeDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using l10n_util::GetNSString;

namespace {
// Matcher for Google Services Settings page.
id<GREYMatcher> GoogleServicesSettingsButton() {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_sufficientlyVisible(),
                    grey_accessibilityID(kSettingsGoogleServicesCellId), nil);
}

// Dismisses the sign-out dialog.
void DismissSignOut() {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap the tools menu to dismiss the popover.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
}

}  // namespace

// Integration tests using the Google services settings screen with
// |kMobileIdentityConsistency| enabled.
@interface GoogleServicesSettingsMICETestCase : WebHttpServerChromeTestCase
@end

@implementation GoogleServicesSettingsMICETestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(signin::kMobileIdentityConsistency);
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tests that disabling the "Allow Chrome sign-in" > "Sign out" option blocks
// the user from signing in to Chrome through the promo sign-in until it is
// re-enabled.
- (void)testToggleAllowChromeSigninWithPromoSignin {
  // User is signed-in only
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Turn off "Allow Chrome Sign-in" with sign out option.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];

  // Verify that sign-in is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_notVisible()];

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Turn on "Allow Chrome Sign-in" feature.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Verify that the user is signed out and sign-in is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that disabling the "Allow Chrome sign-in" > "Clear Data" option blocks
// the user from signing in to Chrome through the promo sign-in until it is
// re-enabled.
- (void)testToggleAllowChromeSigninWithPromoSigninClearData {
  // User is signed-in and syncing.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kWaitForActionTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Turn off "Allow Chrome Sign-in" feature with Clear Data option.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON)]
      performAction:grey_tap()];

  // Verify that sign-in is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_notVisible()];

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Turn on "Allow Chrome Sign-in" feature.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Verify that the user is signed out and sign-in is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that disabling the "Allow Chrome sign-in" > "Keep Data" option blocks
// the user from signing in to Chrome through the promo sign-in until it is
// re-enabled.
- (void)testToggleAllowChromeSigninWithPromoSigninKeepData {
  // User is signed-in and syncing.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kWaitForActionTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Turn off "Allow Chrome Sign-in" feature with Keep Data option.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON)]
      performAction:grey_tap()];

  // Verify that sign-in is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_notVisible()];

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Turn on "Allow Chrome Sign-in" feature.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Verify that the user is signed out and sign-in is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that disabling the "Allow Chrome sign-in" option blocks the user
// from signing in to Chrome through the default sign-in until it is re-enabled.
- (void)testToggleAllowChromeSigninWithDefaultSignin {
  [ChromeEarlGreyUI openSettingsMenu];
  // Close the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  // Turn off "Allow Chrome Sign-in" feature.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Verify that the user is signed out and sign-in is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_notVisible()];
  [SigninEarlGrey verifySignedOut];

  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  // Turn on "Allow Chrome Sign-in" feature.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Verify that the user is signed out and sign-in is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that canceling the "Allow Chrome sign-in" option does not change the
// user's sign-in state.
- (void)testCancelAllowChromeSignin {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  // Turn off "Allow Chrome Sign-in" feature.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Dismiss the sign-out dialog.
  DismissSignOut();

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Turn off "Allow Chrome Sign-in" feature.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Select "sign out" option then dismiss the sign-out dialog.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];
  DismissSignOut();

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end
