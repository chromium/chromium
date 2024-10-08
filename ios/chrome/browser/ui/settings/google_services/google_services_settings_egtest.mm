// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/supervised_user/core/common/features.h"
#import "components/variations/pref_names.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsSignInRowMatcher;
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

// Waits for the settings done button to be enabled.
void WaitForSettingDoneButton() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForClearBrowsingDataTimeout, condition),
             @"Settings done button not visible");
}

// Sets up the sign-in policy value dynamically at runtime.
void SetSigninEnterprisePolicyValue(BrowserSigninMode signinMode) {
  policy_test_utils::SetPolicy(static_cast<int>(signinMode),
                               policy::key::kBrowserSignin);
}

}  // namespace

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : WebHttpServerChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scrollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

- (void)setUp {
  [super setUp];

  // Some tests change the value of this pref, so reset.
  [ChromeEarlGrey clearUserPrefWithName:prefs::kSigninAllowed];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

- (void)tearDown {
  [super tearDown];
  // Some tests change the value of this pref, so reset.
  [ChromeEarlGrey clearUserPrefWithName:prefs::kSigninAllowed];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  if ([self
          isRunningTest:@selector(testToggleAllowChromeSigninForManagedUser)]) {
    // Enable the feature because the dialog uses a slightly different string if
    // the feature is not enabled.
    config.features_enabled.push_back(kIdentityDiscAccountMenu);
  }
  return config;
}

- (AppLaunchConfiguration)appConfigurationForManagedSignoutTestCase {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.features_enabled.push_back(kClearDeviceDataOnSignOutForManagedUsers);
  return config;
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
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_DETAIL];
}

// Tests that disabling the "Allow Chrome sign-in" blocks the user from signing
// in to Chrome through settings until it is re-enabled.
- (void)testToggleAllowChromeSignin {
  // User is signed-in and syncing.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kWaitForActionTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  // Turn off "Allow Chrome Sign-in" feature, which prompts the user with a
  // confirmation dialog to sign out.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];
  WaitForSettingDoneButton();

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
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  // Turn on "Allow Chrome Sign-in" feature.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Verify that the user is signed out and sign-in is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Similar to `testToggleAllowChromeSignin`, but also verifies that an
// informational message about data loss will be added in the prompt.
- (void)testToggleAllowChromeSigninForManagedUser {
  // Restart the app to enable enable the
  // `kClearDeviceDataOnSignOutForManagedUsers` feature on relaunch.
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:
          [self appConfigurationForManagedSignoutTestCase]];

  // Sign in with a managed identity.
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];

  // Turn off "Allow Chrome Sign-in" feature, which prompts the user with a
  // confirmation dialog to sign out.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(l10n_util::GetNSString(
              IDS_IOS_SIGNOUT_AND_DISALLOW_SIGNIN_CLOSES_TABS_AND_CLEARS_DATA_MESSAGE_WITH_MANAGED_ACCOUNT))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];
  WaitForSettingDoneButton();

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
}

// Tests that canceling the "Allow Chrome sign-in" option does not change the
// user's sign-in state.
- (void)testCancelAllowChromeSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  // Turn off "Allow Chrome Sign-in" feature.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAllowSigninItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Dismiss the sign-out dialog.
  DismissSignOut();

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the "Allow Chrome sign-in" option is disabled for supervised
// users.
- (void)testAllowChromeSigninDisabledForSupervisedUsers {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  id<GREYMatcher> signinMatcher = [self
      cellMatcherWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                detailTextID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL];
  [[EarlGrey selectElementWithMatcher:signinMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert that sign-in cell shows the "On" status instead of the knob.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTING_ON))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the sign-in cell can't be used when sign-in is disabled by policy.
- (void)testSigninDisabledByPolicy {
  // Disable browser sign-in.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kDisabled);

  // Open Google services settings and verify the sign-in cell shows the
  // sign-in cell (even if greyed out).
  [self openGoogleServicesSettings];
  id<GREYMatcher> signinMatcher = [self
      cellMatcherWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                detailTextID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL];
  [[EarlGrey selectElementWithMatcher:signinMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert that sign-in cell shows the "Off" status instead of the knob.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTING_OFF))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the parcel tracking settings row is properly shown.
- (void)testParcelTrackingSetting {
  // Parcel tracking is only enabled in the US.
  [ChromeEarlGrey setStringValue:"us"
               forLocalStatePref:variations::prefs::
                                     kVariationsPermanentOverriddenCountry];

  [self openGoogleServicesSettings];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityLabel(GetNSString(
                  IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)),
              grey_kindOfClassName(@"UITableViewCell"),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the parcel tracking settings row is not shown for non-US countries.
- (void)testParcelTrackingSetting_notShownOutsideUS {
  // Set permanent country to somthing other than the US.
  [ChromeEarlGrey setStringValue:"fr"
               forLocalStatePref:variations::prefs::
                                     kVariationsPermanentOverriddenCountry];

  [self openGoogleServicesSettings];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityLabel(GetNSString(
                  IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)),
              grey_kindOfClassName(@"UITableViewCell"),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notVisible()];
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

// Enables the BrowserSigninMode::kDisabled Enterprise policy that disables
// sign-in entry points across Chrome.
- (void)setUpSigninDisabledEnterprisePolicy {
  NSDictionary* policy = @{
    base::SysUTF8ToNSString(policy::key::kBrowserSignin) :
        [NSNumber numberWithInt:(int)BrowserSigninMode::kDisabled]
  };

  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

// Enables the BrowserSigninMode::kForced Enterprise policy that disables the
// allow sign-in item in Google Service Settings.
- (void)setUpSigninForcedEnterprisePolicy {
  NSDictionary* policy = @{
    base::SysUTF8ToNSString(policy::key::kBrowserSignin) :
        [NSNumber numberWithInt:(int)BrowserSigninMode::kForced]
  };

  [[NSUserDefaults standardUserDefaults]
      setObject:policy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

// Returns grey matcher for a cell with `titleID` and `detailTextID`.
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

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
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

// Returns GREYElementInteraction for `matcher`, with `self.scrollViewMatcher`
// to scroll.
- (GREYElementInteraction*)elementInteractionWithGreyMatcher:
    (id<GREYMatcher>)matcher {
  return [self elementInteractionWithGreyMatcher:matcher
                               scrollViewMatcher:self.scrollViewMatcher];
}

// Returns GREYElementInteraction for a cell based on the title string ID and
// the detail text string ID. `detailTextID` should be set to 0 if it doesn't
// exist in the cell.
- (GREYElementInteraction*)cellElementInteractionWithTitleID:(int)titleID
                                                detailTextID:(int)detailTextID {
  id<GREYMatcher> cellMatcher = [self cellMatcherWithTitleID:titleID
                                                detailTextID:detailTextID];
  return [self elementInteractionWithGreyMatcher:cellMatcher];
}

// Asserts that a cell exists, based on its title string ID and its detail text
// string ID. `detailTextID` should be set to 0 if it doesn't exist in the cell.
- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[self cellElementInteractionWithTitleID:titleID detailTextID:detailTextID]
      assertWithMatcher:grey_notNil()];
}

@end
