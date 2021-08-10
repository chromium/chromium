// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::BookmarkHomeDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using l10n_util::GetNSString;

namespace {

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

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : WebHttpServerChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scrollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

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

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  [self openGoogleServicesSettings];

  // Assert title and accessibility.
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the Google Services settings.
- (void)testOpeningServices {
  [self openGoogleServicesSettings];
  [self assertNonPersonalizedServices];
}

// Tests that the Safe Browsing toggle reflects the current value of the
// Safe Browsing preference, and updating the toggle also updates the
// preference.
- (void)testToggleSafeBrowsing {
  // Start in the default (opted-in) state for Safe Browsing.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];

  [self openGoogleServicesSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                  @"Failed to toggle-off Safe Browsing");

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Open settings again, verify Safe Browsing is still disabled, and re-enable
  // it.
  [self openGoogleServicesSettings];
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to toggle-on Safe Browsing");
}

// Tests that password leak detection can only be toggled if Safe Browsing is
// enabled.
- (void)testTogglePasswordLeakCheck {
  // Ensure that Safe Browsing and password leak detection opt-outs start in
  // their default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordLeakDetectionEnabled];

  // Sign in.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check that the password leak check toggle is both toggled off and disabled.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/NO)] assertWithMatcher:grey_notNil()];

  // Toggle Safe Browsing on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check that the password leak check toggle is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-off password leak checks");

  // Toggle password leak check detection back on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-on password leak checks");
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

#pragma mark - Helpers

// Opens the Google services settings.
- (void)openGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  self.scrollViewMatcher =
      grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Scrolls Google services settings to the top.
- (void)scrollUp {
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.1f, 0.1f)];
}

// Enables the Enterprise policy that disables sign-in entry points across
// Chrome.
- (void)setUpSigninDisabledEnterprisePolicy {
  NSDictionary* policy = @{
    base::SysUTF8ToNSString(policy::key::kBrowserSignin) :
        [NSNumber numberWithInt:(int)BrowserSigninMode::kDisabled]
  };

  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

// Returns grey matcher for a cell with |titleID| and |detailTextID|.
- (id<GREYMatcher>)cellMatcherWithTitleID:(int)titleID
                             detailTextID:(int)detailTextID {
  NSString* accessibilityLabel = GetNSString(titleID);
  if (detailTextID) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                   GetNSString(detailTextID)];
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_kindOfClassName(@"UITableViewCell"),
                    grey_sufficientlyVisible(), nil);
}

// Returns GREYElementInteraction for |matcher|, using |scrollViewMatcher| to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                    scrollViewMatcher:(id<GREYMatcher>)scrollViewMatcher {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

// Returns GREYElementInteraction for |matcher|, with |self.scrollViewMatcher|
// to scroll.
- (GREYElementInteraction*)elementInteractionWithGreyMatcher:
    (id<GREYMatcher>)matcher {
  return [self elementInteractionWithGreyMatcher:matcher
                               scrollViewMatcher:self.scrollViewMatcher];
}

// Returns GREYElementInteraction for a cell based on the title string ID and
// the detail text string ID. |detailTextID| should be set to 0 if it doesn't
// exist in the cell.
- (GREYElementInteraction*)cellElementInteractionWithTitleID:(int)titleID
                                                detailTextID:(int)detailTextID {
  id<GREYMatcher> cellMatcher = [self cellMatcherWithTitleID:titleID
                                                detailTextID:detailTextID];
  return [self elementInteractionWithGreyMatcher:cellMatcher];
}

// Asserts that a cell exists, based on its title string ID and its detail text
// string ID. |detailTextID| should be set to 0 if it doesn't exist in the cell.
- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[self cellElementInteractionWithTitleID:titleID detailTextID:detailTextID]
      assertWithMatcher:grey_notNil()];
}

// Asserts that the non-personalized service section is visible.
- (void)assertNonPersonalizedServices {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL];
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
}

@end
