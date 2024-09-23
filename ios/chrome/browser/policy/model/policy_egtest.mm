// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/history/core/common/pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_switches.h"
#import "components/policy/policy_constants.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/policy/test_support/signature_provider.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/translate/model/translate_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_disable_timer_tracking.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using policy_test_utils::SetPolicy;

namespace {

// TODO(crbug.com/40124201): Add helpers as needed for:
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
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_HEADER_NAME)];
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
                              NSString* containerViewAccessibilityID) {
  // Check if the managed item is shown in the corresponding table view.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(accessibilityID),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(containerViewAccessibilityID)]
      assertWithMatcher:grey_notNil()];

  // Click the info button.
  [ChromeEarlGreyUI tapSettingsMenuButton:grey_accessibilityID(
                                              kTableViewCellInfoButtonViewId)];

  // Check if the contextual bubble is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap outside of the bubble.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewCellInfoButtonViewId)]
      performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

NSString* const kDomain1 = @"domain1.com";
NSString* const kDomain2 = @"domain2.com";

}  // namespace

// Test case to verify that enterprise policies are set and respected.
@interface PolicyTestCase : ChromeTestCase
@end

@implementation PolicyTestCase {
  BOOL _settingsOpened;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> _server;
}

- (void)tearDown {
  if (_settingsOpened) {
    [ChromeEarlGrey dismissSettings];
    [ChromeEarlGreyUI waitForAppToIdle];
  }
  [PolicyAppInterface clearPolicies];
  [super tearDown];
}

- (void)openSettingsMenu {
  [ChromeEarlGreyUI openSettingsMenu];
  _settingsOpened = YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = NoForceRelaunchAndResetState;

  if ([self isRunningTest:@selector(testPopupMenuItemWithUserPolicy)] ||
      [self isRunningTest:@selector(testManagementPageManagedWithUserPolicy)]) {
    config.features_enabled.push_back(
        policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  } else {
    config.features_disabled.push_back(
        policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  }

  return config;
}

// Tests that about:policy is available.
- (void)testAboutPolicy {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_HEADER_NAME)];
}

// Tests changing the DefaultSearchProviderEnabled policy while the settings
// are open updates the UI.
- (void)testDefaultSearchProviderUpdate {
  SetPolicy(true, policy::key::kDefaultSearchProviderEnabled);

  [self openSettingsMenu];

  // Check that the non-managed item is present.
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsSearchEngineCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  SetPolicy(false, policy::key::kDefaultSearchProviderEnabled);

  // After setting the policy to false, the item should be replaced.
  VerifyManagedSettingItem(kSettingsManagedSearchEngineCellId,
                           kSettingsTableViewId);
}

// Tests for the DefaultSearchProviderEnabled policy.
// 1. Test if the policy can be properly set.
// 2. Test the managed UI item and clicking action.
- (void)testDefaultSearchProviderEnabled {
  // Disable default search provider via policy and make sure it does not crash
  // the omnibox UI.
  SetPolicy(false, policy::key::kDefaultSearchProviderEnabled);
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];

  // Open a new tab and verify that the NTP does not crash. Regression test for
  // http://crbug.com/1148903.
  [ChromeEarlGrey openNewTab];

  // Open settings menu.
  [self openSettingsMenu];

  VerifyManagedSettingItem(kSettingsManagedSearchEngineCellId,
                           kSettingsTableViewId);
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
  // Open settings menu and tap password manager.
  [self openSettingsMenu];

  // Mock successful reauth when opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];

  // Open password settings.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsToolbarSettingsButtonId)]
      performAction:grey_tap()];

  VerifyManagedSettingItem(
      kPasswordSettingsManagedSavePasswordSwitchTableViewId,
      kPasswordsSettingsTableViewId);

  // Remove mock to keep the app in the same state as before running the test.
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
}

// Tests for the AutofillAddressEnabled policy Settings UI.
- (void)testAutofillAddressSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillAddressEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:autofill::prefs::kAutofillProfileEnabled],
      @"Preference was unexpectedly true");
  // Open settings menu and tap Address and More setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];

  VerifyManagedSettingItem(kAutofillAddressManagedViewId,
                           kAutofillProfileTableViewID);
}

// Tests for the AutofillCreditCardEnabled policy Settings UI.
- (void)testAutofillCreditCardSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillCreditCardEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:autofill::prefs::kAutofillCreditCardEnabled],
      @"Preference was unexpectedly true");
  // Open settings menu and tap Payment Method setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PaymentMethodsButton()];

  VerifyManagedSettingItem(kAutofillCreditCardManagedViewId,
                           kAutofillCreditCardTableViewId);
}

// Tests for the SavingBrowserHistoryDisabled policy.
- (void)testSavingBrowserHistoryDisabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL("/pony.html");
  const std::string pageText = "pony";

  // Set history to a clean state and verify it is clean.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey resetBrowsingDataPrefs];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
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
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Force the preference to false via policy (enables history).
  SetPolicy(false, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly true");

  // Perform a navigation and make sure history is being saved.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
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
  id<GREYMatcher> toolsMenuMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey selectElementWithMatcher:ToolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  /*amount=*/200)
      onElementWithMatcher:toolsMenuMatcher]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Remove any tranlation related setup properly.
  [TranslateAppInterface tearDown];

  // Enable the policy.
  SetPolicy(true, policy::key::kTranslateEnabled);
}

- (void)testBlockPopupsSettingsUI {
  // Set the policy to int value 2, which stands for "do not allow any site to
  // show popups".
  SetPolicy(2, policy::key::kDefaultPopupsSetting);

  // Open settings menu and tap Content Settings setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ContentSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsBlockPopupsCellId)]
      performAction:grey_tap()];

  VerifyManagedSettingItem(@"blockPopupsContentView_managed",
                           @"block_popups_settings_view_controller");
}

// Tests that the feed is disappearing when the policy is set to false while it
// is visible.
- (void)testDisableContentSuggestions {
  // Relaunch the app with Discover enabled, as it is required for this test.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kEnableFeedAblation);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  NSString* feedTitle = l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(feedTitle),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kNTPCollectionViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  SetPolicy(false, policy::key::kNTPContentSuggestionsEnabled);

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(feedTitle),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  // Open settings menu and check that it is disabled.
  [self openSettingsMenu];
  VerifyManagedSettingItem(kSettingsArticleSuggestionsCellId,
                           kSettingsTableViewId);
}

- (void)testTranslateEnabledSettingsUI {
  // Disable TranslateEnabled policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open settings menu and tap Languages setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:chrome_test_util::LanguagesButton()];

  VerifyManagedSettingItem(kTranslateManagedAccessibilityIdentifier,
                           kLanguageSettingsTableViewAccessibilityIdentifier);
}

// Tests whether the managed item will be shown if a machine level policy is
// set.
- (void)testPopupMenuItemWithMachineLevelPolicy {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPopupMenuToolsMenuActionListId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kTextMenuEnterpriseInfo)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_DESC)];

  // Check the navigation.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          kChromeUIManagementURL)]
      assertWithMatcher:grey_notNil()];
}

// Tests whether the managed item will be shown if UserPolicy is enabled and
// the browser is signed in with a managed account.
- (void)testPopupMenuItemWithUserPolicy {
  // Sign in with a managed account.
  NSString* managedAccountEmail = base::SysUTF8ToNSString(
      base::StrCat({"enterprise@", policy::SignatureProvider::kTestDomain1}));
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPopupMenuToolsMenuActionListId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kTextMenuEnterpriseInfo)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_DESC)];

  // Check the navigation without assert the content (which is done in another
  // test case).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          kChromeUIManagementURL)]
      assertWithMatcher:grey_notNil()];
}

// Tests whether the managed item won't be shown if the browser is signed in
// with a managed account but UserPolicy is disabled.
- (void)testPopupMenuItemWithManagedAccountButUserPolicyDisabled {
  // Sign in with a managed account.
  NSString* managedAccountEmail = base::SysUTF8ToNSString(
      base::StrCat({"enterprise@", policy::SignatureProvider::kTestDomain1}));
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];

  // Scroll to the bottom of the tools menu where the enterprise item is if
  // displayed.
  ScopedDisableTimerTracking disabler;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Check that the enterprise item isn't there.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kTextMenuEnterpriseInfo)]
      assertWithMatcher:grey_nil()];
}

// Tests the chrome://management page when no machine level policy is set.
- (void)testManagementPageUnmanaged {
  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_UNMANAGED_DESC)];
}

// Tests the chrome://management page when one or more machine level policies
// are set.
- (void)testManagementPageManaged {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_MESSAGE)];

  // Open the "learn more" link.
  [ChromeEarlGrey tapWebStateElementWithID:@"learn-more-link"];
}

// Tests the chrome://management page when there are machine level policies.
- (void)testManagementPageManagedWithCBCM {
  _server = std::make_unique<policy::EmbeddedPolicyTestServer>();
  _server->Start();

  // Enable machine level (browser) cloud policies.
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  config.additional_args.push_back("-com.apple.configuration.managed");
  // Use an enrollment token that will start chrome browser cloud management
  // without making network calls.
  config.additional_args.push_back(
      base::StrCat({"<dict><key>CloudManagementEnrollmentToken</key><string>",
                    policy::kInvalidEnrollmentToken, "</string></dict>"}));
  // Use the embedded test server as the policy server.
  config.additional_args.push_back(
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    _server->GetServiceURL().spec()}));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [PolicyAppInterface setBrowserCloudPolicyDataWithDomain:kDomain1];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringFUTF8(
                                        IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                        base::SysNSStringToUTF16(kDomain1))];
}

// Tests the chrome://management page when there are user level policies.
- (void)testManagementPageManagedWithUserPolicy {
  // Sign in with a managed account.
  NSString* managedAccountEmail =
      [@"enterprise@" stringByAppendingString:kDomain1];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
                                    base::SysNSStringToUTF16(kDomain1))];
}

// Tests the chrome://management page when there are machine level policies and
// user level policies from the same domain.
- (void)testManagementPageManagedWithCBCMAndUserPolicyDifferentDomains {
  _server = std::make_unique<policy::EmbeddedPolicyTestServer>();
  _server->Start();

  // Enable browser cloud policies.
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  config.additional_args.push_back("-com.apple.configuration.managed");
  // Use a CBCM enrollment token that will start chrome browser cloud management
  // without making network calls.
  config.additional_args.push_back(
      base::StrCat({"<dict><key>CloudManagementEnrollmentToken</key><string>",
                    policy::kInvalidEnrollmentToken, "</string></dict>"}));
  // Use the embedded test server as the policy server.
  config.additional_args.push_back(
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    _server->GetServiceURL().spec()}));
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninOrSyncConsentLevel);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Set CBCM policies.
  [PolicyAppInterface setBrowserCloudPolicyDataWithDomain:kDomain1];

  // Sign in with managed account to enable User Policy.
  NSString* managedAccountEmail =
      [@"enterprise@" stringByAppendingString:kDomain2];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(
              IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
              base::SysNSStringToUTF16(kDomain1),
              base::SysNSStringToUTF16(kDomain2))];
}

// Tests the chrome://management page when there are machine level policies and
// user level policies from different domains.
- (void)testManagementPageManagedWithCBCMAndUserPolicySameDomains {
  _server = std::make_unique<policy::EmbeddedPolicyTestServer>();
  _server->Start();

  // Enable browser cloud policies.
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  config.additional_args.push_back("-com.apple.configuration.managed");
  // Use a CBCM enrollment token that will start chrome browser cloud management
  // without making network calls.
  config.additional_args.push_back(
      base::StrCat({"<dict><key>CloudManagementEnrollmentToken</key><string>",
                    policy::kInvalidEnrollmentToken, "</string></dict>"}));
  // Use the embedded test server as the policy server.
  config.additional_args.push_back(
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    _server->GetServiceURL().spec()}));
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninOrSyncConsentLevel);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Set CBCM policies.
  [PolicyAppInterface setBrowserCloudPolicyDataWithDomain:kDomain1];

  // Sign in with managed account to enable User Policy.
  NSString* managedAccountEmail =
      [@"enterprise@" stringByAppendingString:kDomain1];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey signinWithFakeIdentity:fakeManagedIdentity];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(
              IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
              base::SysNSStringToUTF16(kDomain1))];
}

// Tests that when the BrowserSignin policy is updated while the app is not
// launched, a policy screen is displayed at startup.
- (void)testBrowserSignInDisabledAtStartup {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Create the config to relaunch Chrome.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Configure the policy to disable SignIn.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>0</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  // Add the switch to make sure that fakeIdentity1 is known at startup to avoid
  // automatic sign out.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Check that the sign out pop up is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_SIGNED_OUT))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Signed Out' prompt not shown");
}

// Tests that the UI notifying the user of their sign out is displayed when the
// policy changes while the app is launched.
- (void)testBrowserSignInDisabledWhileAppVisible {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Force sign out.
  SetPolicy(0, policy::key::kBrowserSignin);

  // Check that the sign out pop up is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_SIGNED_OUT))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Signed Out' prompt not shown");
}

// Tests that the UI notifying the user of their sign out is displayed when the
// primary account is restricted.
- (void)testBrowserAccountRestrictedAlert {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Set restrictions.
  base::Value::List restrictions;
  restrictions.Append("restricted");
  SetPolicy(base::Value(std::move(restrictions)),
            policy::key::kRestrictAccountsToPatterns);

  // Check that the sign out pop up is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_SIGNED_OUT))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Signed Out' prompt not shown");
}

// Tests that the UI notifying the user is displayed when sync is disabled by an
// administrator while the app is launched.
- (void)testSyncDisabledPromptWhileAppVisible {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Enable SyncDisabled policy.
  SetPolicy(true, policy::key::kSyncDisabled);

  // Check that the prompt is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    NSString* noticeTitle =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SYNC_DISABLED_TITLE_WITH_UNO);
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(noticeTitle)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Sync Disabled' prompt not shown");
}

// Tests enterprise mode in the Privacy Safe Browsing settings as if the
// enterprise selected Enhanced Protection as the choice of protection.
- (void)testEnhancedSafeBrowsing {
  SetPolicy(2, policy::key::kSafeBrowsingProtectionLevel);
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:chrome_test_util::ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];

  // Tap the info button on row. Accessibility point has been changed in this
  // TableViewInfoButtonItem to be on the center of the row instead of on the
  // "i" button. To tap the "i" button, we select the info button as the matcher
  // instead of the row.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(grey_accessibilityID(
                         kSettingsSafeBrowsingStandardProtectionCellId)),
                     grey_accessibilityID(kTableViewCellInfoButtonViewId),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Check if the contextual bubble is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap outside of the bubble.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingStandardProtectionCellId)]
      performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

@end
