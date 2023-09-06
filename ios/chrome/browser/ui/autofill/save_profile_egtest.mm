// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_address_profile_modal_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForActionTimeout;

namespace {

// URLs of the test pages.
constexpr char kProfileForm[] = "/autofill_smoke_test.html";
constexpr char kFormHTMLFile[] = "/profile_form.html";

// Ids of fields in the form.
constexpr char kFormElementName[] = "name";
constexpr char kFormElementSubmit[] = "submit_profile";

// Matcher for the banner button.
id<GREYMatcher> BannerButtonMatcher() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION));
}

// Matcher for the "Save Address" modal button.
id<GREYMatcher> ModalButtonMatcher() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the modal button.
id<GREYMatcher> ModalEditButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kInfobarSaveAddressModalEditButton),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the migration button in modal view.
id<GREYMatcher> ModalMigrationButtonMatcher() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL)),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Waits for the keyboard to appear. Returns NO on timeout.
BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

}  // namepsace

@interface SaveProfileEGTest : ChromeTestCase

@end

@implementation SaveProfileEGTest

- (void)tearDown {
  // Clear existing profile.
  [AutofillAppInterface clearProfilesStore];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testUserData_MigrationToAccount)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAccountProfileStorage);
    config.features_enabled.push_back(
        syncer::kSyncEnableContactInfoDataTypeInTransportMode);
  }

  return config;
}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitForm {
  [ChromeEarlGrey tapWebStateElementWithID:@"fill_profile_president"];
  [ChromeEarlGrey tapWebStateElementWithID:@"submit_profile"];
}

#pragma mark - Tests

// Ensures that the profile is saved to Chrome after submitting the form.
- (void)testUserData_LocalSave {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [self fillAndSubmitForm];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Ensures that the profile is saved to Chrome after submitting and editing the
// form.
- (void)testUserData_LocalEdit {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [self fillAndSubmitForm];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"New York")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Ensures that the profile is saved to Account after submitting the form.
- (void)testUserData_AccountSave {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [self fillAndSubmitForm];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  id<GREYMatcher> footerMatcher = grey_text(l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, u"foo1@gmail.com"));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that the profile is saved to Account after submitting and editing the
// form.
- (void)testUserData_AccountEdit {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [self fillAndSubmitForm];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"New York")];

  id<GREYMatcher> footerMatcher = grey_text(l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, u"foo1@gmail.com"));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that if a local profile is filled in a form and submitted, the user
// is asked for a migration prompt and the profile is moved to the Account.
- (void)testUserData_MigrationToAccount {
  [AutofillAppInterface clearProfilesStore];

  // Store one local address.
  [AutofillAppInterface saveExampleProfile];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kFormHTMLFile)];

  // Ensure there is a saved local profile.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"There should a saved local profile.");

  // Tap on a field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  WaitForKeyboardToAppear();

  // Tap on the suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillSuggestionViewMatcher()]
      performAction:grey_tap()];

  // Verify Web Content was filled.
  NSString* name = [AutofillAppInterface exampleProfileName];
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value === '%@'",
                       kFormElementName, name];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementSubmit)];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  id<GREYMatcher> footerMatcher = grey_text(l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER, u"foo1@gmail.com"));
  // Check if there is footer suggesting it's a migration prompt.
  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm to migrate the profile.
  [[EarlGrey selectElementWithMatcher:ModalMigrationButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the count of profiles saved remains unchanged.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

@end
