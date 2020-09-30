// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/translate/translate_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a JSON-encoded string representing the given |base::Value|. If
// |value| is nullptr, returns a string representing a |base::Value| of type
// NONE.
NSString* SerializeValue(const base::Value value) {
  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(std::move(value));
  return base::SysUTF8ToNSString(serialized_value);
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be serialized as a JSON string.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(NSString* json_value, const std::string& policy_key) {
  [PolicyAppInterface setPolicyValue:json_value
                              forKey:base::SysUTF8ToNSString(policy_key)];
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be wrapped in a |base::Value|.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(base::Value value, const std::string& policy_key) {
  SetPolicy(SerializeValue(std::move(value)), policy_key);
}

// Sets the value of the policy with the |policy_key| key to the given boolean
// value.
void SetPolicy(bool enabled, const std::string& policy_key) {
  SetPolicy(base::Value(enabled), policy_key);
}

// TODO(crbug.com/1065522): Add helpers as needed for:
//    - INTEGER
//    - STRING
//    - LIST (and subtypes, e.g. int list, string list, etc)
//    - DICTIONARY (and subtypes, e.g. int dictionary, string dictionary, etc)
//    - Deleting a policy value
//    - Setting multiple policies at once

// Verifies that a bool type policy sets the pref properly.
void VerifyBoolPolicy(const std::string& policy_key,
                      const std::string& pref_name) {
  // Loading chrome://policy isn't necessary for the test to succeed, but it
  // provides some visual feedback as the test runs.
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
  // Force the preference off via policy.
  SetPolicy(false, policy_key);
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:pref_name],
                  @"Preference was unexpectedly true");

  // Force the preference on via policy.
  SetPolicy(true, policy_key);
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:pref_name],
                 @"Preference was unexpectedly false");
}

// Returns a matcher for the Translate manual trigger button in the tools menu.
id<GREYMatcher> ToolsMenuTranslateButton() {
  return grey_allOf(grey_accessibilityID(kToolsMenuTranslateId),
                    grey_interactable(), nil);
}

// Verifies that a managed setting item is shown and react properly.
void VerifyManagedSettingItem(NSString* accessibilityID,
                              NSString* accessibilityValue,
                              id<GREYMatcher> goBackMatcher) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(accessibilityID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Click the info button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityValue(accessibilityValue)]
      performAction:grey_tap()];
  // Check if the contextual bubble is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap outside of the bubble.
  [[EarlGrey selectElementWithMatcher:goBackMatcher] performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

}  // namespace

// Test case to verify that enterprise policies are set and respected.
@interface PolicyTestCase : ChromeTestCase
@end

@implementation PolicyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableEnterprisePolicy);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Tests that about:policy is available.
- (void)testAboutPolicy {
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
}

// Tests for the DefaultSearchProviderEnabled policy.
// 1. Test if the policy can be properly set.
// 2. Test the managed UI item and clicking action.
- (void)testDefaultSearchProviderEnabled {
  // Disable default search provider via policy and make sure it does not crash
  // the omnibox UI.
  SetPolicy(false, policy::key::kDefaultSearchProviderEnabled);
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [EarlGrey dismissKeyboardWithError:nil];

  // Open settings menu.
  [ChromeEarlGreyUI openSettingsMenu];

  VerifyManagedSettingItem(
      kSettingsManagedSearchEngineCellId,
      l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_DISABLED_STATUS),
      chrome_test_util::SettingsDoneButton());
}

// Tests for the PasswordManagerEnabled policy.
- (void)testPasswordManagerEnabled {
  VerifyBoolPolicy(policy::key::kPasswordManagerEnabled,
                   password_manager::prefs::kCredentialsEnableService);
}

// Tests for the PasswordManagerEnabled policy Settings UI.
- (void)testPasswordManagerEnabledSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kPasswordManagerEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:password_manager::prefs::kCredentialsEnableService],
      @"Preference was unexpectedly true");
  // Open settings menu and tap password settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsPasswordsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kSettingsTableViewId)]
      performAction:grey_tap()];

  VerifyManagedSettingItem(kSavePasswordManagedTableViewId,
                           l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
                           chrome_test_util::SettingsMenuBackButton());
}

// Tests for the SavingBrowserHistoryDisabled policy.
- (void)testSavingBrowserHistoryDisabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL("/pony.html");
  const std::string pageText = "pony";

  // Set history to a clean state and verify it is clean.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey resetBrowsingDataPrefs];
  GREYAssertEqual([ChromeEarlGrey getBrowsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Verify that the unmanaged pref's default value is false. While we generally
  // don't want to assert default pref values, in this case we need to start
  // from a well-known default value due to the order of the checks we make for
  // the history panel. If the default value ever changes for this pref, we'll
  // need to adjust the order of the history panel checks.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Unexpected default value");

  // Force the preference to true via policy (disables history).
  SetPolicy(true, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly false");

  // Perform a navigation and make sure the history isn't changed.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
  GREYAssertEqual([ChromeEarlGrey getBrowsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Force the preference to false via policy (enables history).
  SetPolicy(false, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly true");

  // Perform a navigation and make sure history is being saved.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
  GREYAssertEqual([ChromeEarlGrey getBrowsingHistoryEntryCount], 1,
                  @"History had an unexpected entry count");
}

// Tests for the SearchSuggestEnabled policy.
- (void)testSearchSuggestEnabled {
  VerifyBoolPolicy(policy::key::kSearchSuggestEnabled,
                   prefs::kSearchSuggestEnabled);
}

// Tests that language detection is not performed and the tool manual trigger
// button is disabled when the pref kOfferTranslateEnabled is set to false.
- (void)testTranslateEnabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL("/pony.html");
  const std::string pageText = "pony";

  // Set up a fake language detection observer.
  [TranslateAppInterface
      setUpWithScriptServer:base::SysUTF8ToNSString(testURL.spec())];

  // Disable TranslateEnabled policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open some webpage.
  [ChromeEarlGrey loadURL:testURL];

  // Check that no language has been detected.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for language detection"
                  block:^BOOL() {
                    return [TranslateAppInterface isLanguageDetected];
                  }];

  GREYAssertFalse([condition waitWithTimeout:2],
                  @"The Language is unexpectedly detected.");

  // Make sure the Translate manual trigger button disabled.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:ToolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  /*amount=*/200)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Remove any tranlation related setup properly.
  [TranslateAppInterface tearDown];

  // Enable the policy.
  SetPolicy(true, policy::key::kTranslateEnabled);
}

// Test whether the managed item will be shown if a policy is set.
- (void)testPopupMenuItem {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:grey_accessibilityID(kTextMenuEnterpriseInfo)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Check the navigation.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "chrome://management")]
      assertWithMatcher:grey_notNil()];
}

// Test the chrome://management page when no machine level policy is set.
- (void)testManagementPageUnmanaged {
  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL("chrome://management")];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_UNMANAGED_DESC)];
}

// Test the chrome://management page when one or more machine level policies are
// set.
- (void)testManagementPageManaged {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL("chrome://management")];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_MESSAGE)];

  // Open the "learn more" link.
  [ChromeEarlGrey tapWebStateElementWithID:@"learn-more-link"];
}

@end
