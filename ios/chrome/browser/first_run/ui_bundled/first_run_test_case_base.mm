// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_test_case_base.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/sync/service/sync_prefs.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_app_interface.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

@implementation FirstRunTestCaseBase

+ (void)dismissDefaultBrowser {
  id<GREYMatcher> buttonMatcher = grey_allOf(
      grey_ancestor(grey_accessibilityID(
          first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)),
      grey_accessibilityTrait(UIAccessibilityTraitStaticText),
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION)),
      nil);

  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

#pragma mark - XCTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");

  // Because this test suite changes the state of Sync passwords, wait
  // until the engine is initialized before startup.
  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
}

- (void)tearDown {
  [SigninEarlGrey signOut];

  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearFakeSyncServerData];

  // Clear sync prefs for data types.
  [ChromeEarlGrey
      clearUserPrefWithName:syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                syncer::UserSelectableType::kTabs)];
  [ChromeEarlGrey
      clearUserPrefWithName:syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                syncer::UserSelectableType::kHistory)];

  // Clear MSBB consent.
  [ChromeEarlGrey
      clearUserPrefWithName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];

  [super tearDown];
}

#pragma mark - BaseEarlGreyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunches the app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark - Helpers

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
      subtitle = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
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
      subtitle = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
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
      subtitle = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
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

- (void)acceptSyncOrHistory {
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
      performAction:grey_tap()];
}

- (void)verifySyncOrHistoryEnabled:(BOOL)enabled {
  if (enabled) {
    GREYAssertTrue([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                   @"History sync was unexpectedly disabled.");
  } else {
    GREYAssertFalse([ChromeEarlGrey isSyncHistoryDataTypeSelected],
                    @"History sync was unexpectedly enabled.");
  }
}

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

- (void)relaunchAppWithBrowserSigninMode:(BrowserSigninMode)mode {
  std::string xmlPolicyValue("<integer>");
  xmlPolicyValue += base::NumberToString(static_cast<int>(mode));
  xmlPolicyValue += "</integer>";
  [self relaunchAppWithPolicyKey:policy::key::kBrowserSignin
                  xmlPolicyValue:xmlPolicyValue];
}

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

- (void)verifyDefaultBrowserIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
