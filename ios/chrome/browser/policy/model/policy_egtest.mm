// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/history/core/common/pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/policy/test_support/signature_provider.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/enterprise/data_protection/test/data_protection_app_interface.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/settings/password/public/passwords_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/elements_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/language/language_settings_ui_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/translate/model/translate_app_interface.h"
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
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForClearBrowsingDataTimeout;
using policy_test_utils::SetPolicy;

namespace {

// Opens the History UI, verifies whether `kHistoryToolbarEditButtonIdentifier`
// is enabled or disabled, and closes the History UI.
void VerifyHistoryUIForAllowDeletingBrowserHistory(bool expected_enabled) {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];

  id<GREYMatcher> edit_button_state_matcher =
      expected_enabled
          ? grey_allOf(grey_enabled(), grey_sufficientlyVisible(), nil)
          : grey_allOf(grey_not(grey_enabled()), grey_sufficientlyVisible(),
                       nil);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryToolbarEditButtonIdentifier)]
      assertWithMatcher:edit_button_state_matcher];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHistoryNavigationControllerDoneButtonIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:grey_accessibilityID(
                                                 kHistoryTableViewIdentifier)];
}

// Opens Quick Delete (Clear Browsing Data), navigates to the Browsing Data
// selection page, verifies whether the Browsing History row is enabled and
// selected or disabled and unselected, and dismisses Quick Delete without
// deleting the seeded history entries.
void VerifyQuickDeleteForAllowDeletingBrowserHistory(bool expected_enabled) {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:chrome_test_util::ButtonWithAccessibilityLabel(
                             l10n_util::GetNSString(
                                 IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA))];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::ClearBrowsingDataView()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_text(l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING))
                                     timeout:kWaitForClearBrowsingDataTimeout];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::BrowsingDataButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
              IDS_IOS_DELETE_BROWSING_DATA_TITLE)];

  NSString* expected_counter_subtitle = l10n_util::GetNSStringF(
      IDS_DEL_BROWSING_HISTORY_COUNTER_SINGLE_DOMAIN_TEXT, u"example.com");
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(
              grey_ancestor(chrome_test_util::ClearBrowsingHistoryButton()),
              chrome_test_util::ContainsPartialText(expected_counter_subtitle),
              nil)];
  id<GREYMatcher> history_row_state_matcher =
      expected_enabled
          ? grey_allOf(grey_enabled(),
                       grey_accessibilityTrait(UIAccessibilityTraitSelected),
                       grey_sufficientlyVisible(), nil)
          : grey_allOf(
                grey_accessibilityTrait(UIAccessibilityTraitNotEnabled),
                grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)),
                grey_sufficientlyVisible(), nil);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ClearBrowsingHistoryButton()]
      assertWithMatcher:history_row_state_matcher];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:
                        chrome_test_util::ButtonWithAccessibilityLabelId(
                            IDS_IOS_DELETE_BROWSING_DATA_CANCEL)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::ClearBrowsingDataView()];
}

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

  // Create a specific matcher for the info button inside the target cell.
  id<GREYMatcher> infoButtonMatcher =
      grey_allOf(grey_accessibilityID(kTableViewCellInfoButtonViewId),
                 grey_ancestor(grey_accessibilityID(accessibilityID)),
                 grey_sufficientlyVisible(), nil);

  // Click the info button.
  [[[EarlGrey selectElementWithMatcher:infoButtonMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(containerViewAccessibilityID)]
      performAction:grey_tap()];

  // Check if the contextual bubble is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap outside of the bubble (tapping the info button again toggles it off).
  [[EarlGrey selectElementWithMatcher:infoButtonMatcher]
      performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

ElementSelector* VisibleElementSelector(NSString* element_id) {
  NSString* selector =
      [NSString stringWithFormat:
                    @"(function() {"
                     "  var element = document.getElementById('%@');"
                     "  if (element == null) return false;"
                     "  if (element.classList.contains('hidden')) return false;"
                     "  return true;"
                     "})()",
                    element_id];
  NSString* description =
      [NSString stringWithFormat:@"Visible element with id: %@.", element_id];
  return [ElementSelector selectorWithScript:selector
                         selectorDescription:description];
}

NSString* const kDomain1 = @"domain1.com";
NSString* const kDomain2 = @"domain2.com";
constexpr char kEnrollmentToken[] = "fake-enrollment-token";

// Path to the pony page.
const char kTestPagePath[] = "/pony.html";
// The text on the pony page.
const char kTestPageText[] = "pony";

// Timeout for the initial page load away from the native New Tab Page, allowing
// WebKit helper processes to launch when exceeding `kWaitForUIElementTimeout`.
constexpr base::TimeDelta kInitialPageLoadTimeout = base::Seconds(20);

}  // namespace

// Test case to verify that enterprise policies are set and respected.
@interface PolicyTestCase : ChromeTestCase
@end

@implementation PolicyTestCase {
  BOOL _settingsOpened;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> _server;
}

- (void)tearDownHelper {
  if (_settingsOpened) {
    [ChromeEarlGrey dismissSettings];
    [ChromeEarlGreyUI waitForAppToIdle];
  }
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

- (void)openSettingsMenu {
  [ChromeEarlGreyUI openSettingsMenu];
  _settingsOpened = YES;
}

- (void)openAutofillSettingsPage {
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(
                                kSettingsAutofillAndPasswordsCellId)];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}));
  config.relaunch_policy = NoForceRelaunchAndResetState;
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
  [self openAutofillSettingsPage];

  // Mock successful reauth when opening the Password Manager.
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
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
}

// Tests for the AutofillAddressEnabled policy Settings UI.
- (void)testAutofillAddressSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillAddressEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:autofill::prefs::kAutofillProfileEnabled],
      @"Preference was unexpectedly true");
  // Open settings menu and tap Address and More setting.
  [self openAutofillSettingsPage];
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
  [self openAutofillSettingsPage];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PaymentMethodsButton()];

  VerifyManagedSettingItem(kAutofillCreditCardManagedViewId,
                           kAutofillCreditCardTableViewId);
}

// Tests for the SavingBrowserHistoryDisabled policy.
- (void)testSavingBrowserHistoryDisabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL(kTestPagePath);

  // Set history to a clean state and verify it is clean.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

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
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Force the preference to false via policy (enables history).
  SetPolicy(false, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly true");

  // Perform a navigation and make sure history is being saved.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History had an unexpected entry count");
}

// Tests for the AllowDeletingBrowserHistory policy across unset, false, and
// true states on History UI (including dynamically exiting active Edit mode)
// and Quick Delete (Clear Browsing Data).
- (void)testAllowDeletingBrowserHistory {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Set history to a clean state and verify it is clean.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

  [ChromeEarlGrey resetBrowsingDataPrefs];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Perform a navigation and seed a domain visit so History UI and Quick Delete
  // (`HistoryCounter` unique domains) both display non-empty browsing history.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  [ChromeEarlGrey addHistoryServiceTypedURL:GURL("https://example.com/")];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 2,
                  @"History had an unexpected entry count");

  // Unset state (default allows deleting browser history).
  [PolicyAppInterface clearPolicies];
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForUIElementTimeout,
                     ^{
                       return [ChromeEarlGrey
                           userBooleanPref:prefs::kAllowDeletingBrowserHistory];
                     }),
                 @"Unexpected default value for AllowDeletingBrowserHistory");
  VerifyQuickDeleteForAllowDeletingBrowserHistory(/*expected_enabled=*/true);

  // Verify History UI in unset state, enter active Edit mode, then dynamically
  // set AllowDeletingBrowserHistory to false and verify History UI exits active
  // Edit mode and disables the Edit button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryToolbarEditButtonIdentifier)]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryToolbarEditButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistoryToolbarDeleteButtonIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  SetPolicy(false, policy::key::kAllowDeletingBrowserHistory);
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout,
          ^{
            return ![ChromeEarlGrey
                userBooleanPref:prefs::kAllowDeletingBrowserHistory];
          }),
      @"AllowDeletingBrowserHistory preference was unexpectedly true");
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistoryToolbarDeleteButtonIdentifier)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryToolbarEditButtonIdentifier)]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHistoryNavigationControllerDoneButtonIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:grey_accessibilityID(
                                                 kHistoryTableViewIdentifier)];

  // False (disabled) state across History UI and Quick Delete.
  VerifyHistoryUIForAllowDeletingBrowserHistory(/*expected_enabled=*/false);
  VerifyQuickDeleteForAllowDeletingBrowserHistory(/*expected_enabled=*/false);

  // True (enabled) state across History UI and Quick Delete.
  SetPolicy(true, policy::key::kAllowDeletingBrowserHistory);
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout,
          ^{
            return [ChromeEarlGrey
                userBooleanPref:prefs::kAllowDeletingBrowserHistory];
          }),
      @"AllowDeletingBrowserHistory preference was unexpectedly false");
  VerifyHistoryUIForAllowDeletingBrowserHistory(/*expected_enabled=*/true);
  VerifyQuickDeleteForAllowDeletingBrowserHistory(/*expected_enabled=*/true);
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
  const GURL testURL = self.testServer->GetURL(kTestPagePath);

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
      grey_accessibilityID(kPopupMenuToolsMenuActionListId);
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
  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUIManagementURL)];
}

// Tests whether the managed item will be shown if UserPolicy is enabled and
// the browser is signed in with a managed account.
- (void)testPopupMenuItemWithUserPolicy {
  // Sign in with a managed account.
  NSString* managedAccountEmail = base::SysUTF8ToNSString(
      base::StrCat({"enterprise@", policy::SignatureProvider::kTestDomain1}));
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPopupMenuToolsMenuActionListId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kTextMenuEnterpriseInfo)];

  // Check the navigation before waiting for the page content.
  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_DESC)];
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

  // Enable machine level (browser) cloud policies. Use an enrollment token that
  // will start chrome browser cloud management without making network calls.
  AppLaunchConfiguration config;
  policy_test_utils::AddCBCMEnrollmentArguments(
      config, policy::kInvalidEnrollmentToken, _server->GetServiceURL());
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
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
                                    base::SysNSStringToUTF16(kDomain1))];
}

// Tests the chrome://management page when reporting is disabled.
- (void)testManagementPageManagedWithoutReporting {
  // Open the management page.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_MANAGEMENT_BROWSER_REPORTING)];

  // Expect the profile reporting section to be invisible.
  GREYAssertFalse(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  @"profile-reporting-info")],
      @"Profile reporting section is visible.");
}

// Tests the chrome://management page when browser reporting is enabled.
- (void)testManagementPageManagedWithBrowserReporting {
  // Set up profile reporting.
  SetPolicy(true, policy::key::kCloudReportingEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION)];
}

// Tests the chrome://management page when profile reporting is enabled.
- (void)testManagementPageManagedWithProfileReporting {
  // Set up profile reporting.
  SetPolicy(true, policy::key::kCloudProfileReportingEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_MANAGEMENT_PROFILE_REPORTING_EXPLANATION)];
}

// Tests the chrome://management page when browser reporting is enabled.
- (void)testManagementPageManagedWithBrowserAndProfileReporting {
  // Set up profile reporting.
  SetPolicy(true, policy::key::kCloudReportingEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey waitForWebStateContainingText:
                      l10n_util::GetStringUTF8(
                          IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION)];

  // If both are enabled, only show the browser reporting section.
  GREYAssertFalse(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  @"profile-reporting-info")],
      @"Profile reporting section is visible.");
}

// Tests the chrome://management page when there are machine level policies and
// user level policies from the same domain.
- (void)testManagementPageManagedWithCBCMAndUserPolicyDifferentDomains {
  _server = std::make_unique<policy::EmbeddedPolicyTestServer>();
  _server->Start();

  // Enable browser cloud policies. Use a CBCM enrollment token that will start
  // chrome browser cloud management without making network calls.
  AppLaunchConfiguration config;
  policy_test_utils::AddCBCMEnrollmentArguments(
      config, policy::kInvalidEnrollmentToken, _server->GetServiceURL());
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Set CBCM policies.
  [PolicyAppInterface setBrowserCloudPolicyDataWithDomain:kDomain1];

  // Sign in with managed account to enable User Policy.
  NSString* managedAccountEmail =
      [@"enterprise@" stringByAppendingString:kDomain2];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

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

  // Enable browser cloud policies. Use a CBCM enrollment token that will start
  // chrome browser cloud management without making network calls.
  AppLaunchConfiguration config;
  policy_test_utils::AddCBCMEnrollmentArguments(
      config, policy::kInvalidEnrollmentToken, _server->GetServiceURL());
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Set CBCM policies.
  [PolicyAppInterface setBrowserCloudPolicyDataWithDomain:kDomain1];

  // Sign in with managed account to enable User Policy.
  NSString* managedAccountEmail =
      [@"enterprise@" stringByAppendingString:kDomain1];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity identityWithEmail:managedAccountEmail];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(
              IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
              base::SysNSStringToUTF16(kDomain1))];
}

// Tests the chrome://management page when the security event connector is
// enabled.
- (void)testManagementPageManagedSecurityEventConnector {
  _server =
      enterprise_connectors::test::CreatePolicyTestServerForSecurityEvents();
  CHECK(_server);
  CHECK(_server->Start());

  AppLaunchConfiguration config;
  policy_test_utils::AddCBCMEnrollmentArguments(config, kEnrollmentToken,
                                                _server->GetServiceURL());

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];

  // Check that the management domain is part of the connectors disclaimer.
  [ChromeEarlGrey
      waitForWebStateContainingText:
          l10n_util::GetStringFUTF8(
              IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY, u"example.com")];

  // Tap the button to show more info.
  [ChromeEarlGrey
      waitForWebStateContainingElement:VisibleElementSelector(
                                           @"threat-protection-more")];
  [ChromeEarlGrey tapWebStateElementWithID:@"threat-protection-more"];

  // Validate that the Connectors section is visible.
  GREYAssertTrue(
      [ChromeEarlGrey
          webStateContainsElement:VisibleElementSelector(@"connectors-info")],
      @"Connectors section is not visible.");

  // Validate that the security event section is visible.
  GREYAssertTrue(
      [ChromeEarlGrey webStateContainsElement:VisibleElementSelector(
                                                  @"security-event-section")],
      @"Enterprise Security event section not shown.");
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
                                   test_switches::kAddFakeIdentitiesAtStartup);

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
// policy changes while the app is launched.
- (void)testBrowserSignInDisabledWhileAppVisibleFromManagedAccount {
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

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
  base::ListValue restrictions;
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
// enterprise selected No Protection as the choice of protection.
- (void)testEnterpriseBubbleInEnhancedSafeBrowsingPage {
  SetPolicy(0, policy::key::kSafeBrowsingProtectionLevel);
  // Under Chrome Next, the NTP's tools menu button may be scrolled offscreen.
  // Load a standard page to ensure a visible tools menu button.
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    [ChromeEarlGrey loadURL:GURL(kChromeUIVersionURL)];
  }
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:chrome_test_util::ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];

  // Tap the info button on row. Accessibility point has been changed in this
  // TableViewInfoButtonItem to be on the center of the row instead of on the
  // "i" button. To tap the "i" button, we select the info button as the
  // matcher instead of the row.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(grey_accessibilityID(
                         kSettingsSafeBrowsingEnhancedProtectionCellId)),
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
          grey_accessibilityID(kSettingsSafeBrowsingEnhancedProtectionCellId)]
      performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

// Tests print option is visible in share menu if PrintingEnabled is set to true
// (default) and not visible when PrintingEnabled policy is set to false
// (disabled).
- (void)testPrintingDisabledInShare {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL(kTestPagePath);

  // Set and verify that PrintingEnabled pref is set to false correctly.
  SetPolicy(true, policy::key::kPrintingEnabled);
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kPrintingEnabled],
                 @"PrintingEnabled preference was unexpectedly false");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open the share menu and tap the "more" button to check for the "print"
  // option if we are on iOS 26. Other iOS version does not require tapping the
  // "more" button to see the "print" option.
  [ChromeEarlGreyUI openShareMenu];
  [ChromeEarlGrey verifyActivitySheetVisible];
  XCUIApplication* app = [[XCUIApplication alloc] init];
  [ChromeEarlGrey tapMoreOptionButtonInActivitySheet];

  // Check that "print" option exist.
  XCUIElementQuery* print = [[app staticTexts] matchingIdentifier:@"Print"];
  GREYAssertTrue(print.count == 1,
                 @"'Print' option should have been on the share menu");
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  [ChromeEarlGrey verifyActivitySheetNotVisible];

  // Set and verify that PrintingEnabled pref is set to false correctly.
  SetPolicy(false, policy::key::kPrintingEnabled);
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kPrintingEnabled],
                  @"PrintingEnabled preference was unexpectedly true");

  [ChromeEarlGreyUI openShareMenu];
  [ChromeEarlGrey verifyActivitySheetVisible];

  [ChromeEarlGrey tapMoreOptionButtonInActivitySheet];

  // Check that "print" option does not exist.
  print = [[app staticTexts] matchingIdentifier:@"Print"];
  GREYAssertTrue(print.count == 0,
                 @"'Print' option should not have been on the share menu");
}

// Tests that Full Page screenshot PDF generation is allowed when the
// `DisableScreenshots` enterprise policy is unset.
- (void)testDisableScreenshotsUnset {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)
              withTimeout:kInitialPageLoadTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertTrue(
      [DataProtectionAppInterface canGenerateFullPageScreenshotPDF],
      @"Full Page screenshot PDF should be generated when policy is unset.");
}

// Tests that Full Page screenshot PDF generation is blocked in both regular and
// Incognito tabs when the `DisableScreenshots` enterprise policy is set to
// `true`.
- (void)testDisableScreenshotsEnabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  SetPolicy(true, policy::key::kDisableScreenshots);

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)
              withTimeout:kInitialPageLoadTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertFalse(
      [DataProtectionAppInterface canGenerateFullPageScreenshotPDF],
      @"Full Page screenshot PDF should be blocked when DisableScreenshots is "
      @"true.");

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)
              withTimeout:kInitialPageLoadTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertFalse(
      [DataProtectionAppInterface canGenerateFullPageScreenshotPDF],
      @"Full Page screenshot PDF should be blocked in Incognito when "
      @"DisableScreenshots is true.");
}

// Tests that Full Page screenshot PDF generation is allowed when the
// `DisableScreenshots` enterprise policy is set to `false`.
- (void)testDisableScreenshotsDisabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  SetPolicy(false, policy::key::kDisableScreenshots);

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPagePath)
              withTimeout:kInitialPageLoadTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageText];
  GREYAssertTrue(
      [DataProtectionAppInterface canGenerateFullPageScreenshotPDF],
      @"Full Page screenshot PDF should be generated when DisableScreenshots "
      @"is false.");
}

// Tests that the dinosaur game is playable on `chrome://dino/` when the
// `AllowDinosaurEasterEgg` enterprise policy is set to `true`.
- (void)testAllowDinosaurEasterEggEnabled {
  SetPolicy(true, policy::key::kAllowDinosaurEasterEgg);
  [ChromeEarlGrey loadURL:GURL(kChromeDinoGameURL)
              withTimeout:kInitialPageLoadTimeout];
  ElementSelector* containerSelector =
      [ElementSelector selectorWithCSSSelector:".runner-container"];
  [ChromeEarlGrey waitForWebStateContainingElement:containerSelector];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithCSSSelector:".runner-canvas"]];
  [ChromeEarlGrey waitForWebStateNotContainingElement:
                      [ElementSelector selectorWithCSSSelector:".snackbar"]];
  [ChromeEarlGrey
      waitForWebStateNotContainingText:l10n_util::GetStringUTF8(
                                           IDS_ERRORPAGE_FUN_DISABLED)];

  // Tapping the runner container starts the game.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified(
                        containerSelector)];
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithCSSSelector:".controller"]];
}

// Tests that `chrome://dino/` shows the disabled page instead of the dinosaur
// game when the `AllowDinosaurEasterEgg` enterprise policy is set to `false`.
- (void)testAllowDinosaurEasterEggDisabled {
  SetPolicy(false, policy::key::kAllowDinosaurEasterEgg);
  [ChromeEarlGrey loadURL:GURL(kChromeDinoGameURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_ERRORPAGE_FUN_DISABLED)];
  [ChromeEarlGrey
      waitForWebStateNotContainingElement:
          [ElementSelector selectorWithCSSSelector:".runner-canvas"]];

  // Pressing space on the disabled page shows the disabled message and icon.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@" " flags:0];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithCSSSelector:".snackbar.snackbar-show"]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithCSSSelector:".icon.icon-disabled"]];
}

@end
