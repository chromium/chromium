// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_app_interface.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_disable_timer_tracking.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonStackSecondaryButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;

namespace {

/// User name and passwords for conflicted passwords in the valid ZIP file.
NSString* const kURL = @"https://sebsg.github.io/";
NSString* const kUsername1 = @"Homer_Simpson";
NSString* const kUsername2 = @"Superman";
NSString* const kPassword2 = @"LouisLane";

/// User name for the invalid password.
NSString* const kInvalidPasswordUsername = @"Superman";

}  // namespace

/// Tests for Safari data import.
@interface SafariDataImportTestCase : ChromeTestCase
@end

@implementation SafariDataImportTestCase

/// Helper method creating an instance of AppLaunchConfiguration that enables
/// the Safari import feature by default. No other behavior is overridden.
- (AppLaunchConfiguration)appConfigurationNoOverrideBehavior {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kImportPasswordsFromSafari);
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

/// App configuration for this test suite, which ensures that the Safari import
/// entry point displays on app startup in non-first-run launches. This reduces
/// the time it takes for each test case by removing the need to go through FRE
/// screens or go into Settings.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [self appConfigurationNoOverrideBehavior];
  config.additional_args.push_back("-NextPromoForDisplayOverride");
  config.additional_args.push_back(
      "promos_manager::Promo::SafariImportRemindMeLater");
  return config;
}

- (void)setUp {
  [super setUp];
  /// Clear existing data.
  [ChromeEarlGrey clearBrowsingHistory];
  [BookmarkEarlGrey clearBookmarks];
  [PasswordManagerAppInterface clearCredentials];
  [AutofillAppInterface clearCreditCardStore];
}

/// Verify that the current number of items in the storage matches the
/// parameters.
- (void)verifyItemCountForBookmarks:(int)bookmarksCount
                          passwords:(int)passwordsCount
                         creditCard:(int)ccCount {
  /// TODO(crbug.com/450598882): Add verification for history entries.
  if (bookmarksCount == 0) {
    [BookmarkEarlGrey
        verifyAbsenceOfFolderWithTitle:@"Favorites"
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  } else {
    [BookmarkEarlGrey verifyChildCount:bookmarksCount
                      inFolderWithName:@"Favorites"
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  }
  GREYAssertEqual([PasswordManagerAppInterface storedCredentialsCount],
                  passwordsCount,
                  @"Number of imported passwords do not match.");
  GREYAssertEqual([AutofillAppInterface localCreditCount], ccCount,
                  @"Number of imported credit cards do not match.");
}

#pragma mark - Test cases

/// Tests that the Safari import entry point is displayed on first run and on
/// reminder.
- (void)testFirstRunAndReminder {
  if (@available(iOS 18.2, *)) {
    [[self class] testForStartup];
    AppLaunchConfiguration firstRunConfig =
        [self appConfigurationNoOverrideBehavior];
    /// Show the First Run UI at startup.
    firstRunConfig.additional_args.push_back("-FirstRunForceEnabled");
    firstRunConfig.additional_args.push_back("true");
    firstRunConfig.relaunch_policy = ForceRelaunchByCleanShutdown;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:firstRunConfig];
    /// Go through first run screens by tapping the secondary action twice
    /// (skipping default browser settings and sign-in.)
    id<GREYMatcher> buttonMatcher = ButtonStackSecondaryButton();
    id<GREYMatcher> scrollViewMatcher =
        grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
    id<GREYAction> searchAction =
        grey_scrollInDirection(kGREYDirectionDown, 200);
    for (int i = 0; i < 2; i++) {
      GREYElementInteraction* element =
          [[EarlGrey selectElementWithMatcher:buttonMatcher]
                 usingSearchAction:searchAction
              onElementWithMatcher:scrollViewMatcher];
      [element performAction:grey_tap()];
    }
    /// Verify the visibility of the entry point, and register reminder.
    SetReminderOnSafariDataImportEntryPoint();

    /// Restart in IPH demo mode. If the reminder is successfully registered, it
    /// will be displayed on restart.
    AppLaunchConfiguration iphConfig =
        [self appConfigurationNoOverrideBehavior];
    iphConfig.iph_feature_enabled = "IPH_iOSSafariImportFeature";
    iphConfig.relaunch_policy = ForceRelaunchByCleanShutdown;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:iphConfig];
    DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
  }
}

/// Tests that the entry point to import Safari data can be triggered through
/// Settings.
- (void)testShowEntryPointInSettings {
  if (@available(iOS 18.2, *)) {
    /// Clean restart without experimental settings.
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:
            [self appConfigurationNoOverrideBehavior]];
    [ChromeEarlGreyUI openSettingsMenu];
    {
      ScopedDisableTimerTracking disabler;
      [[[EarlGrey selectElementWithMatcher:
                      grey_allOf(grey_accessibilityID(
                                     kSettingsSafariDataImportSettingsCellId),
                                 grey_interactable(), nil)]
             usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
          onElementWithMatcher:grey_accessibilityID(kSettingsTableViewId)]
          performAction:grey_tap()];
      /// Verify visibility and that the reminder button is not displaying.
      GREYAssertTrue(IsSafariDataImportEntryPointVisible(),
                     @"Safari data import workflow is not displayed.");
      [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                              ButtonStackTertiaryButton()]
          assertWithMatcher:grey_notVisible()];
      /// Also verify that swipe would not be supported.
      [[EarlGrey selectElementWithMatcher:
                     grey_accessibilityID(
                         kConfirmationAlertTitleAccessibilityIdentifier)]
          performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
      DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityID(kSettingsTableViewId)]
          assertWithMatcher:grey_sufficientlyVisible()];
    }
  }
}

/// Tests that the entry point displays in full screen in landscape mode.
- (void)testFullscreenEntryPointIPhoneLandscapeMode {
  if (@available(iOS 18.2, *)) {
    if ([ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_SKIPPED(@"Inapplicable on iPad.");
    }
    /// Verify that NTP logo is visible before rotation.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_sufficientlyVisible()];
    /// Rotate.
    [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                     error:nil];
    /// Verify that NTP logo is mostly hidden after rotation.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    /// Verify that the user is still able to proceed in landscape mode.
    StartImportOnSafariDataImportEntryPoint();
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        assertWithMatcher:grey_minimumVisiblePercent(0.5f)];
  }
}

/// Tests that tapping the "Cancel" button on the export screen dismisses the
/// workflow in entirety.
- (void)testDismissFromExportScreen {
  if (@available(iOS 18.2, *)) {
    StartImportOnSafariDataImportEntryPoint();
    /// Swipe should not be supported.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        assertWithMatcher:grey_minimumVisiblePercent(0.5f)];
    /// Dismiss by tapping the "Cancel" button on the top right.
    id<GREYMatcher> buttonInNavBar = grey_allOf(
        grey_kindOfClass([UIButton class]),
        grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
    [[EarlGrey selectElementWithMatcher:buttonInNavBar]
        performAction:grey_tap()];
    /// Verify that NTP logo is interactable, which means that the entry point
    /// is dismissed.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_interactable()];
    GREYAssertFalse(IsSafariDataImportEntryPointVisible(),
                    @"Safari data import workflow is not fully dismissed.");
  }
}

/// Tests that the workflow imports all data from Safari without displaying the
/// password conflict resolution table when there is no password conflicts.
- (void)testNoPasswordConflict {
  if (@available(iOS 18.2, *)) {
    GoToImportScreen();
    LoadFile(SafariDataImportTestFile::kValid);
    /// Check that the items table has displayed.
    ExpectImportTableHasRowCount(4);
    /// Import the data and wait until completion.
    [[EarlGrey selectElementWithMatcher:
                   ImportScreenButtonWithTextId(
                       IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_IMPORT)]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        ImportScreenButtonWithTextId(
                            IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_DONE)];
    ExpectImportTableHasRowCount(4);
    /// Check invalid passwords.
    TapInfoButtonForInvalidPasswords(3, 1);
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                            kInvalidPasswordUsername)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey
        selectElementWithMatcher:
            StaticTextWithAccessibilityLabelId(
                IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_MISSING_URL)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey
        selectElementWithMatcher:
            grey_buttonTitle(l10n_util::GetNSString(
                IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_BUTTON_CLOSE))]
        performAction:grey_tap()];
    /// Dismiss the workflow. Verify that NTP logo is interactable, which means
    /// that the entry point is dismissed.
    CompletesImportWorkflow();
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_interactable()];
    GREYAssertFalse(IsSafariDataImportEntryPointVisible(),
                    @"Safari data import workflow is not fully dismissed.");
    [self verifyItemCountForBookmarks:4 passwords:3 creditCard:3];
  }
}

/// Tests that the workflow will display the table for the user to resolve
/// password conflicts, if there is any,
- (void)testPasswordConflictResolution {
  if (@available(iOS 18.2, *)) {
    [PasswordSettingsAppInterface setUpMockReauthenticationModule];
    [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];
    /// Store some password that will result in a conflict.
    NSString* existingPassword = @"Google!Password)";
    NSURL* url = [NSURL URLWithString:kURL];
    [PasswordManagerAppInterface storeCredentialWithUsername:kUsername1
                                                    password:existingPassword
                                                         URL:url];
    [PasswordManagerAppInterface storeCredentialWithUsername:kUsername2
                                                    password:existingPassword
                                                         URL:url];
    /// Start the flow.
    GoToImportScreen();
    LoadFile(SafariDataImportTestFile::kValid);
    ExpectImportTableHasRowCount(4);
    /// Import the data and check conflict resolution page.
    [[EarlGrey selectElementWithMatcher:
                   ImportScreenButtonWithTextId(
                       IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_IMPORT)]
        performAction:grey_tap()];
    id<GREYMatcher> conflictResolutionTable = grey_accessibilityID(
        GetPasswordConflictResolutionTableViewAccessibilityIdentifier());
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:conflictResolutionTable];
    /// Tests password reveal.
    id<GREYMatcher> row2 = grey_accessibilityID(
        GetPasswordConflictResolutionTableViewCellAccessibilityIdentifier(1));
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_ancestor(row2),
                                     grey_accessibilityID(kShowActionSymbol),
                                     nil)] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_text(kPassword2)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_ancestor(row2),
                                     grey_accessibilityID(kHideActionSymbol),
                                     nil)] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_text(kPassword2)]
        assertWithMatcher:grey_nil()];
    /// Tests "(de)select all" button.
    id<GREYMatcher> select_all = grey_allOf(
        grey_buttonTitle(l10n_util::GetNSString(
            IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_SELECT_ALL)),
        grey_interactable(), nil);
    id<GREYMatcher> deselect_all = grey_allOf(
        grey_buttonTitle(l10n_util::GetNSString(
            IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_DESELECT_ALL)),
        grey_interactable(), nil);
    [[EarlGrey selectElementWithMatcher:select_all] performAction:grey_tap()];
    ExpectPasswordConflictCellAtIndexSelected(0, YES);
    ExpectPasswordConflictCellAtIndexSelected(1, YES);
    [[EarlGrey selectElementWithMatcher:deselect_all] performAction:grey_tap()];
    ExpectPasswordConflictCellAtIndexSelected(0, NO);
    ExpectPasswordConflictCellAtIndexSelected(1, NO);
    /// Select all and deselect one row.
    [[EarlGrey selectElementWithMatcher:select_all] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_allOf(grey_ancestor(row2),
                                                   grey_selected(), nil)]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:select_all]
        assertWithMatcher:grey_sufficientlyVisible()];
    ExpectPasswordConflictCellAtIndexSelected(0, YES);
    ExpectPasswordConflictCellAtIndexSelected(1, NO);
    [[EarlGrey selectElementWithMatcher:grey_buttonTitle(l10n_util::GetNSString(
                                            IDS_CONTINUE))]
        performAction:grey_tap()];
    /// Dismiss the workflow after import completes. Verify that NTP logo is
    /// interactable, which means that the entry point is dismissed.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        ImportScreenButtonWithTextId(
                            IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_DONE)];
    ExpectImportTableHasRowCount(4);
    CompletesImportWorkflow();
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_interactable()];
    GREYAssertFalse(IsSafariDataImportEntryPointVisible(),
                    @"Safari data import workflow is not fully dismissed.");
    [self verifyItemCountForBookmarks:4 passwords:3 creditCard:3];
    /// Verify the right password is overridden.
    [PasswordManagerAppInterface
        verifyCredentialStoredWithUsername:kUsername1
                                  password:existingPassword];
    [PasswordManagerAppInterface verifyCredentialStoredWithUsername:kUsername2
                                                           password:kPassword2];
    [PasswordSettingsAppInterface removeMockReauthenticationModule];
  }
}

/// Tests uploading a file that only contains potential "history" items.
- (void)testUploadPartiallyValidFile {
  if (@available(iOS 18.2, *)) {
    GoToImportScreen();
    LoadFile(SafariDataImportTestFile::kPartiallyValid);
    ExpectImportTableHasRowCount(4);
    [[EarlGrey selectElementWithMatcher:
                   ImportScreenButtonWithTextId(
                       IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_IMPORT)]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        ImportScreenButtonWithTextId(
                            IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_DONE)];
    ExpectImportTableHasRowCount(1);
    CompletesImportWorkflow();
    [self verifyItemCountForBookmarks:0 passwords:0 creditCard:0];
  }
}

/// Tests uploading an invalid file.
- (void)testUploadInvalidFile {
  if (@available(iOS 18.2, *)) {
    GoToImportScreen();
    LoadFile(SafariDataImportTestFile::kInvalid);
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:
            StaticTextWithAccessibilityLabelId(
                IDS_IOS_SAFARI_IMPORT_IMPORT_FAILURE_MESSAGE_DESCRIPTION)];
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(IDS_OK))]
        performAction:grey_tap()];
    /// Try selecting a valid file. It should proceed.
    LoadFile(SafariDataImportTestFile::kValid);
    ExpectImportTableHasRowCount(4);
    [self verifyItemCountForBookmarks:0 passwords:0 creditCard:0];
  }
}

@end
