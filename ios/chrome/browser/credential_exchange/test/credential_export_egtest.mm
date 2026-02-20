// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/credential_exchange/public/metrics.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_constants.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using ::password_manager_test_utils::OpenPasswordManager;
using ::password_manager_test_utils::SaveExamplePasskeyToStore;
using ::password_manager_test_utils::SavePasswordFormToAccountStore;

#if BUILDFLAG(IOS_CREDENTIAL_EXCHANGE_ENABLED)
// Matcher for the continue button.
id<GREYMatcher> ContinueButton() {
  return grey_accessibilityID(
      kCredentialExportContinueButtonAccessibilityIdentifier);
}

// Matcher for the "Export data" button.
id<GREYMatcher> ExportButtonMatcher() {
  return grey_accessibilityID(kPasswordSettingsCredentialExportButtonId);
}

// Matcher for the Toggle button (Select All / Deselect All) using its ID.
id<GREYMatcher> ToggleSelectionButton() {
  return grey_accessibilityID(
      kCredentialExportSelectAllButtonAccessibilityIdentifier);
}

// Matcher for the specific state text "Select All".
id<GREYMatcher> SelectAllText() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_SELECT_ALL_BUTTON));
}

// Matcher for the specific state text "Deselect All".
id<GREYMatcher> DeselectAllText() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_DESELECT_ALL_BUTTON));
}

// Matcher for the "Export" button.
id<GREYMatcher> ExportOptionsButton() {
  return grey_accessibilityID(
      kCredentialExportFileButtonAccessibilityIdentifier);
}

// Matcher for the "Download to CSV" menu item.
id<GREYMatcher> DownloadCsvMenuAction() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_EXPORT_PASSWORDS_DOWNLOAD_CSV)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for a UITableViewCell containing specific text.
id<GREYMatcher> CellWithText(NSString* text) {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_descendant(grey_text(text)),
                    grey_sufficientlyVisible(), nil);
}

// Opens the Credential Export page from Password Settings.
void OpenExportCredentialsPage() {
  OpenPasswordManager();

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsToolbarSettingsButtonId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordsSettingsTableViewId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ExportButtonMatcher()]
      performAction:grey_tap()];
}

// Verifies that credential export screen action histogram was recorded.
void CheckCredentialExportScreenActionMetric(
    CredentialExportScreenAction action) {
  NSString* histogram =
      base::SysUTF8ToNSString(kCredentialExportScreenActionHistogram);
  NSError* error = [MetricsAppInterface expectCount:1
                                          forBucket:static_cast<int>(action)
                                       forHistogram:histogram];
  GREYAssertNil(error, @"Failed to record credential export screen histogram.");
}
#endif

}  // namespace

// Integration Tests for Credential Export View Controller.
@interface CredentialExportTestCase : ChromeTestCase
@end

@implementation CredentialExportTestCase

- (void)setUp {
  [super setUp];

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

- (void)tearDownHelper {
  [PasswordSettingsAppInterface clearPasskeyStore];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

#pragma mark - Tests

#if BUILDFLAG(IOS_CREDENTIAL_EXCHANGE_ENABLED)
// Tests that tapping the Continue button proceeds with the export process.
// TODO(crbug.com/454566693): The OS bottom sheet doesn't seem to appear.
- (void)DISABLED_testTapContinueButton {
  SavePasswordFormToAccountStore(@"password", @"user", @"https://example.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      assertWithMatcher:grey_notVisible()];

  CheckCredentialExportScreenActionMetric(
      CredentialExportScreenAction::kContinuePressed);
}

// Verifies the default state when opening the screen.
- (void)testInitialState {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"password1", @"user1",
                                 @"https://example1.com");
  SavePasswordFormToAccountStore(@"password2", @"user2",
                                 @"https://example2.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:DeselectAllText()];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      assertWithMatcher:grey_enabled()];
}

// Tests the toggle functionality of the "Select All" / "Deselect All" button on
// the export credentials page.
- (void)testToggleSelectAllDeselectAll {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"password1", @"user1",
                                 @"https://example1.com");
  SavePasswordFormToAccountStore(@"password2", @"user2",
                                 @"https://example2.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:SelectAllText()];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:DeselectAllText()];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      assertWithMatcher:grey_enabled()];

  CheckCredentialExportScreenActionMetric(
      CredentialExportScreenAction::kSelectAllPressed);
  CheckCredentialExportScreenActionMetric(
      CredentialExportScreenAction::kDeselectAllPressed);
}

// Tests that the "Download to CSV" button is enabled by default since all
// items, including passwords, are selected initially.
- (void)testDownloadToCSVEnabledByDefault {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SaveExamplePasskeyToStore();
  SavePasswordFormToAccountStore(@"password1", @"user1",
                                 @"https://example1.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:DeselectAllText()];

  [[EarlGrey selectElementWithMatcher:ExportOptionsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DownloadCsvMenuAction()]
      assertWithMatcher:grey_enabled()];

  // Check that metrics are correctly logged on button tap.
  [[EarlGrey selectElementWithMatcher:DownloadCsvMenuAction()]
      performAction:grey_tap()];
  CheckCredentialExportScreenActionMetric(
      CredentialExportScreenAction::kDownloadToCSVPressed);
}

// Tests that the "Download to CSV" button becomes disabled when the
// only available password items are deselected.
- (void)testDownloadToCSVDisabledWhenPasswordDeselected {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SaveExamplePasskeyToStore();
  SavePasswordFormToAccountStore(@"password1", @"user1",
                                 @"https://example1.com");
  OpenExportCredentialsPage();

  // Deselect the password row.
  [[EarlGrey selectElementWithMatcher:CellWithText(@"example1.com")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ExportOptionsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DownloadCsvMenuAction()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Tests that initially all items are selected and the toggle button shows
// "Deselect All".
- (void)testInitialSelectionState {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"pass1", @"user1", @"https://example1.com");
  SavePasswordFormToAccountStore(@"pass2", @"user2", @"https://example2.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:DeselectAllText()];

  [[EarlGrey selectElementWithMatcher:CellWithText(@"example1.com")]
      assertWithMatcher:grey_accessibilityTrait(UIAccessibilityTraitSelected)];
  [[EarlGrey selectElementWithMatcher:CellWithText(@"example2.com")]
      assertWithMatcher:grey_accessibilityTrait(UIAccessibilityTraitSelected)];
}

// Tests that tapping "Deselect All" deselects all items and updates the button
// to "Select All".
- (void)testDeselectAllToggle {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"pass1", @"user1", @"https://example1.com");
  SavePasswordFormToAccountStore(@"pass2", @"user2", @"https://example2.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:SelectAllText()];

  [[EarlGrey selectElementWithMatcher:CellWithText(@"example1.com")]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitSelected))];
  [[EarlGrey selectElementWithMatcher:CellWithText(@"example2.com")]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitSelected))];
}

// Tests that deselecting a single item changes the main toggle button to
// "Select All".
- (void)testPartialDeselectionUpdatesToggleButton {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"pass1", @"user1", @"https://example1.com");
  SavePasswordFormToAccountStore(@"pass2", @"user2", @"https://example2.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:DeselectAllText()];

  [[EarlGrey selectElementWithMatcher:CellWithText(@"example1.com")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      assertWithMatcher:SelectAllText()];
}

// Tests that the navigation bar title updates to reflect the number of selected
// items.
- (void)testTitleReflectsSelectionCount {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"pass1", @"user1", @"https://example1.com");
  SavePasswordFormToAccountStore(@"pass2", @"user2", @"https://example2.com");
  OpenExportCredentialsPage();

  NSString* expectedLabelForTwoSelected = l10n_util::GetPluralNSStringF(
      IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_COUNT, 2);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          expectedLabelForTwoSelected)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:CellWithText(@"example1.com")]
      performAction:grey_tap()];

  NSString* expectedLabelForOneSelected = l10n_util::GetPluralNSStringF(
      IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_COUNT, 1);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          expectedLabelForOneSelected)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:CellWithText(@"example2.com")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ToggleSelectionButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          expectedLabelForTwoSelected)]
      assertWithMatcher:grey_sufficientlyVisible()];
}
#endif

@end
