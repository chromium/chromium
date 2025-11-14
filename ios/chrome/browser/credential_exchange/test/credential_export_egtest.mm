// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_constants.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
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

// Matcher for the "Export Passwords and Passkeys" button.
id<GREYMatcher> ExportButtonMatcher() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS));
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

// Opens the Credential Export page from Password Settings.
void OpenExportCredentialsPage() {
  OpenPasswordManager();

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsToolbarSettingsButtonId)]
      performAction:grey_tap()];

  id<GREYMatcher> scrollViewMatcher = grey_kindOfClass([UIScrollView class]);

  [[[EarlGrey selectElementWithMatcher:ExportButtonMatcher()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:scrollViewMatcher] performAction:grey_tap()];
}
#endif

}  // namespace

// Integration Tests for Credential Export View Controller.
@interface CredentialExportTestCase : ChromeTestCase
@end

@implementation CredentialExportTestCase

- (void)setUp {
  [super setUp];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

#pragma mark - Tests

#if BUILDFLAG(IOS_CREDENTIAL_EXCHANGE_ENABLED)
// TODO(crbug.com/454566693): Add more EGTests.
// Tests that tapping the Continue button proceeds with the export process.
- (void)testTapContinueButton {
  if (!@available(iOS 26, *)) {
    EARL_GREY_TEST_SKIPPED(@"This feature works only for iOS 26 and higher.");
  }
  SavePasswordFormToAccountStore(@"password", @"user", @"https://example.com");
  OpenExportCredentialsPage();

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      assertWithMatcher:grey_notVisible()];
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
}
#endif

@end
