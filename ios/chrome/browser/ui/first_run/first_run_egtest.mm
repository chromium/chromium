// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/number_formatting.h"
#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_prefs.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Type of FRE sign-in screen intent.
typedef NS_ENUM(NSUInteger, FRESigninIntent) {
  // FRE without enterprise policy.
  FRESigninIntentRegular,
  // FRE without forced sign-in policy.
  FRESigninIntentSigninForcedByPolicy,
  // FRE without disabled sign-in policy.
  FRESigninIntentSigninDisabledByPolicy,
  // FRE with an enterprise policy which is not explicitly handled by another
  // entry.
  FRESigninIntentSigninWithPolicy,
  // FRE with the SyncDisabled enterprise policy.
  FRESigninIntentSigninWithSyncDisabledPolicy,
  // FRE with no UMA link in the first screen.
  FRESigninIntentSigninWithUMAReportingDisabledPolicy,
};

NSString* const kSyncPassphrase = @"hello";

// Returns matcher for the sync encryption action button.
id<GREYMatcher> SyncEncryptionButtonMatcher() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_MANAGE_SYNC_ENCRYPTION),
                    grey_sufficientlyVisible(), nil);
}

// Returns matcher for UMA manage link.
id<GREYMatcher> ManageUMALinkMatcher() {
  return grey_allOf(grey_accessibilityLabel(@"Manage"),
                    grey_sufficientlyVisible(), nil);
}

// Returns matcher for the button to open the Sync settings.
id<GREYMatcher> GetSyncSettings() {
  id<GREYMatcher> disclaimer =
      grey_accessibilityID(kPromoStyleDisclaimerViewAccessibilityIdentifier);
  return grey_allOf(grey_accessibilityLabel(@"settings"),
                    grey_ancestor(disclaimer), grey_sufficientlyVisible(), nil);
}

// Selects search engine cell with `search_engine_name`.
void SelectSearchEngineCellWithName(NSString* search_engine_name,
                                    GREYDirection scroll_direction,
                                    CGFloat scroll_amount) {
  NSString* searchEngineAccessibiltyIdentifier =
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 search_engine_name];
  id<GREYMatcher> searchEngineRowMatcher =
      grey_allOf(grey_userInteractionEnabled(),
                 grey_accessibilityID(searchEngineAccessibiltyIdentifier), nil);
  // Scroll down to find the search engine cell.
  id<GREYMatcher> scrollView =
      grey_accessibilityID(kSearchEngineTableViewIdentifier);
  [[[EarlGrey selectElementWithMatcher:searchEngineRowMatcher]
         usingSearchAction:grey_scrollInDirection(scroll_direction,
                                                  scroll_amount)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
  // Tap on the search engine cell.
  [[[EarlGrey selectElementWithMatcher:searchEngineRowMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

// Confirms the search engine choice screen. A search engine must have been
// selected before.
void ConfirmSearchEngineChoiceScreen() {
  id<GREYMatcher> moreButtonMatcher =
      grey_allOf(grey_accessibilityID(kSearchEngineMoreButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    // Tap on the "More" button if it exists.
    [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  }
  // Tap on the "Set as Default" button.
  id<GREYMatcher> primaryButtonMatcher =
      grey_allOf(grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:primaryButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

// Dismisses the choice screen if it appears
void DismissChoiceScreenIfNecessary() {
  if (![ChromeEarlGreyAppInterface IsSearchEngineChoiceScreenEnabledFre]) {
    return;
  }
  // Selects a search engine. The list of search engines varies from country to
  // country and is refreshed periodically. Google should always be proposed in
  // the countries selected by test settings.
  SelectSearchEngineCellWithName(
      @"Google", /*scroll_direction=*/kGREYDirectionDown, /*scroll_amount*/ 50);
  ConfirmSearchEngineChoiceScreen();
}

// Dismisses the remaining screens in FRE after the search engine choice screen.
void DismissScreensAfterChoiceScreen() {
  id<GREYMatcher> buttonMatcher = grey_allOf(
      grey_ancestor(grey_accessibilityID(
          first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)),
      grey_accessibilityTrait(UIAccessibilityTraitStaticText),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION)),
      nil);

  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];

  if ([FirstRunAppInterface isOmniboxPositionChoiceEnabled]) {
    id<GREYMatcher> omniboxPositionScreenPrimaryButton = grey_allOf(
        grey_ancestor(grey_accessibilityID(
            first_run::
                kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier)),
        grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier),
        nil);

    [[[EarlGrey selectElementWithMatcher:omniboxPositionScreenPrimaryButton]
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  }
}

// Dismisses the remaining screens in FRE after sign-in and sync.
void DismissScreensAfterSigninAndSync() {
  DismissChoiceScreenIfNecessary();
  DismissScreensAfterChoiceScreen();
}

}  // namespace

// Test first run stages
@interface FirstRunTestCase : ChromeTestCase

@end

@implementation FirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];

  // Because this test suite changes the state of Sync passwords, wait
  // until the engine is initialized before startup.
  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
}

- (void)tearDown {
  [SigninEarlGrey signOut];

  // Tests that use `addBookmarkWithSyncPassphrase` must ensure that Sync
  // data is cleared before tear down to reset the Sync password state.
  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearSyncServerData];

  // Clear sync prefs for data types.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                    syncer::UserSelectableType::kTabs))];
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                    syncer::UserSelectableType::kHistory))];

  // Clear MSBB consent.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                unified_consent::prefs::
                                    kUrlKeyedAnonymizedDataCollectionEnabled)];

  // Clear the "choice was made" timestamp pref.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:
          base::SysUTF8ToNSString(
              prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enable the choice screen feature flag.
  config.features_enabled.push_back(switches::kSearchEngineChoiceFre);
  // Set the country to one that is eligible for the choice screen (in this
  // case, France).
  config.additional_args.push_back("--search-engine-choice-country=FR");

  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunches the app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  if ([self isRunningTest:@selector(testSignInWithNoAccount)] ||
      [self isRunningTest:@selector(testHistorySyncSkipIfNoSignIn)] ||
      [self isRunningTest:@selector(testHistorySyncShownAfterSignIn)] ||
      [self isRunningTest:@selector
            (testSignInSubtitleIfHistorySyncOptInEnabled)] ||
      [self
          isRunningTest:@selector(testHistorySyncConsentGrantedAfterConfirm)] ||
      [self isRunningTest:@selector
            (testHistorySyncConsentNotGrantedAfterReject)] ||
      [self isRunningTest:@selector(testHistorySyncSkipIfSyncDisabled)] ||
      [self isRunningTest:@selector(testHistorySyncSkipIfTabsSyncDisabled)] ||
      [self isRunningTest:@selector
            (testHistorySyncShownIfBookmarksSyncDisabled)] ||
      [self isRunningTest:@selector(testHistorySyncLayout)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else if ([self isRunningTest:@selector
                   (testAdvancedSettingsWithSyncPassphrase)] ||
             [self isRunningTest:@selector
                   (testAdvancedSettingsAndDisableTwoDataTypes)] ||
             [self isRunningTest:@selector
                   (testSigninWithOnlyBookmarkSyncDataTypeEnabled)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  return config;
}

#pragma mark - Tests

// Tests FRE with UMA default value and without sign-in.
- (void)testWithUMACheckedAndNoSignin {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndNoSignin {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off, reopen UMA dialog and close the FRE without sign-in.
- (void)testUMAUncheckedWhenOpenedSecondTime {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];

  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  [ChromeEarlGreyUI waitForAppToIdle];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Open UMA dialog again.
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check UMA off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests to turn off UMA, and open the UMA dialog to turn it back on.
// TODO(crbug.com/1487756): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testUMAUncheckedAndCheckItAgain \
  DISABLED_testUMAUncheckedAndCheckItAgain
#else
#define MAYBE_testUMAUncheckedAndCheckItAgain testUMAUncheckedAndCheckItAgain
#endif
- (void)MAYBE_testUMAUncheckedAndCheckItAgain {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Open UMA dialog again.
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Turn UMA back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndSignin {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

// Tests FRE with UMA default value and with sign-in.
- (void)testWithUMACheckedAndSignin {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

// Tests FRE with UMA default value, with sign-in and no sync.
- (void)testWithUMACheckedAndSigninAndNoSync {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is off.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
}

// Tests accepting sync with 2 datatype disabled.
- (void)testAdvancedSettingsAndDisableTwoDataTypes {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Turn off "Sync Everything".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSyncEverythingItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Turn off "Address and more".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncAutofillIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Turn off "Bookmarks".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isInitialSyncFeatureSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  // Check "Sync Everything" is off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSyncEverythingItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
  // Check "Address and more" is off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncAutofillIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
  // Check "Bookmarks" is off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

// Tests sign-in in FRE with an identity that needs a sync passphrase.
- (void)testAdvancedSettingsWithSyncPassphrase {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kSyncPassphrase];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Select Encryption item.
  [[self elementInteractionWithGreyMatcher:SyncEncryptionButtonMatcher()
                      scrollViewIdentifier:
                          kManageSyncTableViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGreyUI submitSyncPassphrase:kSyncPassphrase];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

#pragma mark - Enterprise

// Tests FRE with disabled sign-in policy.
- (void)testSignInDisabledByPolicy {
  // Configure the policy to disable SignIn.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kDisabled];
  // Verify 2 step FRE with disabled sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninDisabledByPolicy];
  // Accept FRE.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests forced sign-in policy, and accept sync.
- (void)testForceSigninByPolicy {
  // Configure the policy to force sign-in.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kForced];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests forced sign-in policy, and refuse sync.
- (void)testForceSigninByPolicyWithoutSync {
  // Configure the policy to force sign-in.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kForced];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests sign-in with sync disabled policy.
- (void)testSyncDisabledByPolicy {
  [self relaunchAppWithPolicyKey:policy::key::kSyncDisabled
                  xmlPolicyValue:"<true/>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithSyncDisabledPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
}

// Tests sign-in and no sync with forced policy.
- (void)testSigninWithOnlyBookmarkSyncDataTypeEnabled {
  // Configure the policy to force sign-in.
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>bookmarks</string></array>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Check "Sync Everything" is off
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kSyncEverythingItemAccessibilityIdentifier),
                            grey_descendant(grey_text(
                                l10n_util::GetNSString(IDS_IOS_SETTING_OFF))),
                            nil)] assertWithMatcher:grey_notNil()];
  // Check "Bookmarks" is off
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                     kSyncBookmarksIdentifier),
                                                 grey_descendant(grey_text(
                                                     l10n_util::GetNSString(
                                                         IDS_IOS_SETTING_OFF))),
                                                 nil)]
      assertWithMatcher:grey_notNil()];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests enterprise policy wording on FRE when incognito policy is set.
- (void)testIncognitoPolicy {
  // Configure the policy to force sign-in.
  [self relaunchAppWithPolicyKey:policy::key::kIncognitoModeAvailability
                  xmlPolicyValue:"<integer>1</integer>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Refuse sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests that the UMA link does not appear in  FRE when UMA is disabled by
// enterprise policy.
- (void)testUMADisabledByPolicy {
  // Configure the policy to disable UMA.
  [self relaunchAppWithPolicyKey:policy::key::kMetricsReportingEnabled
                  xmlPolicyValue:"<false/>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with no UMA footer.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithUMAReportingDisabledPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

#pragma mark - Supervised User

// TODO(crbug.com/1476928): This test is failing.
// Tests FRE with UMA default value and with sign-in for a supervised user.
- (void)DISABLED_testWithUMACheckedAndSigninSupervised {
  // Add a fake supervised identity to the device.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeSupervisedIdentity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES
                                     forIdentity:fakeSupervisedIdentity];

  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeSupervisedIdentity];
  // Check sync is on.
  DismissScreensAfterSigninAndSync();
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

#pragma mark - Sync UI Disabled

// Tests sign-in with FRE when there's no account on the device.
// See https://crbug.com/1471972.
- (void)testSignInWithNoAccount {
  // Add account.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the primary button text is correct.
  NSString* continueAsText = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(),
                     grey_descendant(grey_text(continueAsText)), nil)]
      assertWithMatcher:grey_notNil()];
  // Sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests if the user skip the Sign-in step, the History Sync Opt-in screen is
// skipped and the default browser screen is shown.
- (void)testHistorySyncSkipIfNoSignIn {
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the search engine choice screen is shown.
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
}

// Tests if the user signs in with the first screen, the History Sync Opt-In
// screen is shown next.
- (void)testHistorySyncShownAfterSignIn {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the footer is shown without the user's email.
  NSString* disclaimerText =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(disclaimerText),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
}

// Tests that the correct subtitle is shown in the FRE sign-in screen if the
// History Sync Opt-In feature is enabled.
- (void)testSignInSubtitleIfHistorySyncOptInEnabled {
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Validate the subtitle text.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and that
// History Sync Opt-In screen is skipped, if the sync is disabled by policy, and
// History Sync Opt-In feature is enabled.
- (void)testHistorySyncSkipIfSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncDisabled
                  xmlPolicyValue:"<true/>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the search engine choice screen is shown.
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and that
// History Sync Opt-In screen is skipped, in case the tabs sync is disabled by
// policy, and History Sync Opt-In feature is enabled.
- (void)testHistorySyncSkipIfTabsSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>tabs</string></array>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the search engine choice screen is shown.
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and
// that History Sync Opt-In screen is shown, in case only the bookmarks sync is
// disabled by policy, and History Sync Opt-In feature is enabled.
- (void)testHistorySyncShownIfBookmarksSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>bookmarks</string></array>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that accepting History Sync enables the history sync, grants the
// history sync and MSBB consent.
- (void)testHistorySyncConsentGrantedAfterConfirm {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the search engine choice screen is shown.
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is granted.
  // Verify that MSBB consent is granted.
  GREYAssertTrue(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent was not granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that refusing History Sync keep the history syncdisabled, and does not
// grant the history sync and MSBB consent.
- (void)testHistorySyncConsentNotGrantedAfterReject {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the search engine choice screen is shown.
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is not granted.
  // Verify that MSBB consent is not granted.
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent should not be granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the History Sync Opt-In screen contains the avatar of the
// signed-in user, and the correct background image for the avatar.
- (void)testHistorySyncLayout {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the user's avatar is shown.
  NSString* avatarLabel =
      [NSString stringWithFormat:@"%@ %@", fakeIdentity.userFullName,
                                 fakeIdentity.userEmail];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(avatarLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the avatar background is shown.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kPromoStyleHeaderViewBackgroundAccessibilityIdentifier),
              chrome_test_util::ImageViewWithImageNamed(
                  @"history_sync_opt_in_background"),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the Search Engine Choice screen is displayed, that the primary
// button is correctly updated when the user selects a search engine then
// scrolls down and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenSelectThenScroll {
  if (![ChromeEarlGreyAppInterface IsSearchEngineChoiceScreenEnabledFre]) {
    // Do not run this test if the choice screen is not enabled.
    return;
  }
  // Skips sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Checks that the choice screen is shown
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
  // Verifies that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = @"Bing";
  SelectSearchEngineCellWithName(searchEngineToSelect,
                                 /*scroll_direction=*/kGREYDirectionDown,
                                 /*scroll_amount*/ 50);
  // Taps the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  // Verify that the "More" button has been removed.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_nil()];
  ConfirmSearchEngineChoiceScreen();
  DismissScreensAfterChoiceScreen();

  [self verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the Search Engine Choice screen is displayed, that the primary
// button is correctly updated when the user scolls down then selects a search
// engine and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenScollThenSelect {
  if (![ChromeEarlGreyAppInterface IsSearchEngineChoiceScreenEnabledFre]) {
    // Do not run this test if the choice screen is not enabled.
    return;
  }
  // Skips sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Checks that the choice screen is shown
  [self verifyChoiceScreenOrDefaultBrowserIsDisplayed];
  // Verifies that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Taps the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];

  // Verifies that the primary button is now the disabled "Set as Default"
  // button.
  id<GREYMatcher> primaryActionButtonMatcher =
      grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier);
  [[EarlGrey selectElementWithMatcher:primaryActionButtonMatcher]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()), grey_notNil(),
                                   nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = @"Bing";
  SelectSearchEngineCellWithName(searchEngineToSelect,
                                 /*scroll_direction=*/kGREYDirectionUp,
                                 /*scroll_amount*/ 300);
  ConfirmSearchEngineChoiceScreen();
  DismissScreensAfterChoiceScreen();
  [self verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

#pragma mark - Helper

- (void)relaunchAppWithBrowserSigninMode:(BrowserSigninMode)mode {
  std::string xmlPolicyValue("<integer>");
  xmlPolicyValue += std::to_string(static_cast<int>(mode));
  xmlPolicyValue += "</integer>";
  [self relaunchAppWithPolicyKey:policy::key::kBrowserSignin
                  xmlPolicyValue:xmlPolicyValue];
}

// Sets policy value and relaunches the app.
- (void)relaunchAppWithPolicyKey:(std::string)policyKey
                  xmlPolicyValue:(std::string)xmlPolicyValue {
  std::string policyData = std::string("<dict><key>") + policyKey + "</key>" +
                           xmlPolicyValue + "</dict>";
  // Configure the policy to force sign-in.
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policyData);
  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Checks that the sign-in screen for enterprise is displayed.
- (void)verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
    (FRESigninIntent)FRESigninIntent {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  NSString* title = nil;
  NSString* subtitle = nil;
  NSArray* disclaimerStrings = nil;
  switch (FRESigninIntent) {
    case FRESigninIntentRegular:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
        subtitle = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
      } else {
        subtitle =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      }
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninForcedByPolicy:
      title =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE_SIGNIN_FORCED);
      subtitle = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SIGNIN_FORCED);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninDisabledByPolicy:
      if ([ChromeEarlGrey isIPadIdiom]) {
        title =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD);
      } else {
        title = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
      }
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninWithSyncDisabledPolicy:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      // Note: With SyncDisabled, the "benefits" string is not used.
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninWithPolicy:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
        subtitle = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
      } else {
        subtitle =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      }
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninWithUMAReportingDisabledPolicy:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
        subtitle = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
      } else {
        subtitle =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      }
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
      ];
      break;
  }
  // Validate the Title text.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(title),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Validate the Subtitle text.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Validate the Managed text.
  [self verifyDisclaimerFooterWithStrings:disclaimerStrings];
}

// Checks the disclaimer footer with the list of strings. `strings` can contain
// "BEGIN_LINK" and "END_LINK" for URL tags.
- (void)verifyDisclaimerFooterWithStrings:(NSArray*)strings {
  NSString* disclaimerText = [strings componentsJoinedByString:@" "];
  // Remove URL tags.
  disclaimerText =
      [disclaimerText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                withString:@""];
  disclaimerText =
      [disclaimerText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                withString:@""];
  // Check the footer.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(disclaimerText),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
}

// Checks that the search engine choice screen is being displayed when the
// feature is enabled. If the feature is not enabled, checks that the default
// browser screen is displayed.
- (void)verifyChoiceScreenOrDefaultBrowserIsDisplayed {
  if ([ChromeEarlGreyAppInterface IsSearchEngineChoiceScreenEnabledFre]) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                first_run::kSearchEngineChoiceTitleAccessibilityIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    return;
  }
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)acceptSyncOrHistory {
  if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    // Accept the history opt-in screen.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
        performAction:grey_tap()];
  } else {
    // Accept sync.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kTangibleSyncViewAccessibilityIdentifier)]
        assertWithMatcher:grey_notNil()];
    [[self elementInteractionWithGreyMatcher:
               chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                        scrollViewIdentifier:
                            kPromoStyleScrollViewAccessibilityIdentifier]
        performAction:grey_tap()];
  }
}

- (void)verifySyncOrHistoryEnabled:(BOOL)enabled {
  if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    if (enabled) {
      GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                     @"History sync was unexpectedly disabled.");
    } else {
      GREYAssertFalse([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                      @"History sync was unexpectedly enabled.");
    }
  } else {
    [SigninEarlGrey verifySyncUIEnabled:enabled];
  }
}

- (void)verifyDefaultSearchEngineSetting:(NSString*)searchEngineName {
  // Opens the default search engine settings menu.
  [ChromeEarlGreyUI openSettingsMenu];
  // Verifies that the correct search engine is selected. The default engine's
  // name appears in the name of the selected row.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSearchEngineButton()]
      assertWithMatcher:grey_allOf(grey_accessibilityValue(searchEngineName),
                                   grey_sufficientlyVisible(), nil)];
}

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                 scrollViewIdentifier:(NSString*)scrollViewIdentifier {
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(scrollViewIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

@end
