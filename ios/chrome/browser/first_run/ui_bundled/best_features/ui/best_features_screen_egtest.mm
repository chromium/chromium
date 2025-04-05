// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/notifications_earl_grey_app_interface.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns a matcher for the Default Browser Promo title.
id<GREYMatcher> DefaultBrowserPromoTitle() {
  return grey_text(l10n_util::GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE));
}

// Returns a matcher for the Lens title.
id<GREYMatcher> LensTitle() {
  return grey_text(l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_TITLE));
}

// Returns a matcher for the Enhanced Safe Browsing title.
id<GREYMatcher> EnhancedSafeBrowsingTitle() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_BEST_FEATURES_ENHANCED_SAFE_BROWSING_TITLE));
}

// Returns a matcher for the Locked Incognito tabs title.
id<GREYMatcher> LockedIncognitoTitle() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_TITLE));
}

// Returns a matcher for the Never Forget Passwords title.
id<GREYMatcher> NeverForgetPasswordsTitle() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_TITLE));
}

// Returns a matcher for the Tab Groups title.
id<GREYMatcher> TabGroupsTitle() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_TITLE));
}

// Returns a matcher for the Price Tracking title.
id<GREYMatcher> PriceTrackingTitle() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_TITLE));
}

// Returns a matcher for the Passwords In Other Apps title.
id<GREYMatcher> PasswordsInOtherAppsTitle() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_TITLE));
}

// Returns a matcher for the Share Passwords title.
id<GREYMatcher> SharePasswordsTitle() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_TITLE));
}

}  // namespace

@interface BestFeaturesScreenTest : ChromeTestCase
@end

// Tests the Best Features screen variants.
@implementation BestFeaturesScreenTest

- (void)setUp {
  [super setUp];
  [self signIn];

  if ([self isRunningTest:@selector(testBestFeatures_variantECPEEnabled)]) {
    // Enable the Credential Provider.
    [ChromeEarlGrey setBoolValue:YES
               forLocalStatePref:password_manager::prefs::
                                     kCredentialProviderEnabledOnStartup];
  }
}

- (void)tearDownHelper {
  [ChromeEarlGrey
      resetDataForLocalStatePref:password_manager::prefs::
                                     kCredentialProviderEnabledOnStartup];
  [SigninEarlGrey signOut];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Disable conflicting experiments/features.
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");
  config.additional_args.push_back(
      "--disable-features=AnimatedDefaultBrowserPromoInFRE");
  config.additional_args.push_back(
      "--disable-features=SegmentedDefaultBrowserPromo");

  // Enable the correct variant.
  std::string feature_param;
  if ([self isRunningTest:@selector(testBestFeatures_variantA)]) {
    feature_param = "1";
  }
  if ([self isRunningTest:@selector(testBestFeatures_variantB)]) {
    feature_param = "2";
  }
  if ([self isRunningTest:@selector(testBestFeatures_variantC)]) {
    feature_param = "3";
  }
  if ([self isRunningTest:@selector(testBestFeatures_variantDShopper)]) {
    feature_param = "4";
    config.additional_args.push_back("-ForceExperienceForShopper");
    config.additional_args.push_back("true");
  }
  if ([self isRunningTest:@selector(testBestFeatures_variantDNonShopper)]) {
    feature_param = "4";
  }
  if ([self isRunningTest:@selector(testBestFeatures_variantE)] ||
      [self isRunningTest:@selector(testBestFeatures_variantECPEEnabled)]) {
    feature_param = "5";
  }

  config.additional_args.push_back(
      "--enable-features=BestFeaturesScreenInFirstRunExperience:"
      "BestFeaturesScreenInFirstRunParam/" +
      feature_param);

  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  return config;
}

#pragma mark - Helpers

// Taps a promo button.
- (void)tapPromoButton:(NSString*)buttonID {
  id<GREYMatcher> buttonMatcher = grey_accessibilityID(buttonID);
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

// Signs in and skips sync.
- (void)signIn {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunSignInScreenAccessibilityIdentifier)];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign in.
  [self tapPromoButton:kPromoStylePrimaryActionAccessibilityIdentifier];
  // Skip history/tab sync.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  [self tapPromoButton:kPromoStyleSecondaryActionAccessibilityIdentifier];
}

// Skips the Default Browser Promo.
- (void)skipDefaultBrowserPromo {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];
  [[EarlGrey selectElementWithMatcher:DefaultBrowserPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Skip the Default Browser Promo to avoid leaving the app.
  [self tapPromoButton:kPromoStyleSecondaryActionAccessibilityIdentifier];

  // Wait for the Best Features Screen to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kBestFeaturesMainScreenAccessibilityIdentifier)];
}

- (void)checkDetailScreen:(int)title_id withStep:(int)step_id {
  NSString* title = l10n_util::GetNSString(title_id);

  // Click on the item.
  [self tapPromoButton:[kBestFeaturesCellAccessibilityPrefix
                           stringByAppendingString:title]];

  // Ensure the Feature Highlight Screenshot view appears.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Click the primary action button and ensure the Instructions Half Sheet View
  // Controller comes up.
  [self tapPromoButton:kConfirmationAlertPrimaryActionAccessibilityIdentifier];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(step_id))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Tests

// Tests that the Best Features screens appears after the Default Browser Promo,
// the items are correct, and that the FeatureHighlightScreenshotVC and
// InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantA {
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:LockedIncognitoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_LENS_TITLE
                 withStep:IDS_IOS_BEST_FEATURES_LENS_STEP_1];
}

// Tests that the Best Features screens appears before the Default Browser
// Promo, the items are correct, and that the FeatureHighlightScreenshotVC and
// InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantB {
  // Wait for the Best Features Screen to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kBestFeaturesMainScreenAccessibilityIdentifier)];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:LockedIncognitoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_ENHANCED_SAFE_BROWSING_TITLE
                 withStep:
                     IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP1];
}

// Tests that the Best Features screens appears after the Default Browser Promo,
// the items are correct, and that the FeatureHighlightScreenshotVC and
// InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantC {
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:NeverForgetPasswordsTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_TITLE
                 withStep:
                     IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_FIRST_STEP];
}

// Tests that the Best Features screens appears after the Default Browser Promo,
// shopping users see the correct items, and that the
// FeatureHighlightScreenshotVC and InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantDShopper {
  [NotificationsEarlGreyAppInterface setUpMockShoppingService];
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:TabGroupsTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:LockedIncognitoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PriceTrackingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_PRICE_TRACKING_TITLE
                 withStep:IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_1];
}

// Tests that the Best Features screens appears after the Default Browser Promo,
// non-shopping users see the correct items, and that the
// FeatureHighlightScreenshotVC and InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantDNonShopper {
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:NeverForgetPasswordsTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_TITLE
                 withStep:
                     IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_FIRST_STEP];
}

// Tests that the Best Features screens appears after the Default Browser Promo,
// users who don't have CPE enabled see the correct items, and that the
// FeatureHighlightScreenshotVC and InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantE {
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_TITLE
                 withStep:IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_1];
}

// Tests that the Best Features screens appears after the Default Browser Promo,
// users who have CPE enabled see the correct items, and that the
// FeatureHighlightScreenshotVC and InstructionsHalfSheetVC appear correctly.
- (void)testBestFeatures_variantECPEEnabled {
  // Ensure the Default Browser Promo appears before the Best Features Screen.
  [self skipDefaultBrowserPromo];

  // Assert that the correct items are visible.
  [[EarlGrey selectElementWithMatcher:LensTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:EnhancedSafeBrowsingTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SharePasswordsTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Best Featues Detail Screen and Instructions Half Sheet
  // appear correctly.
  [self checkDetailScreen:IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_TITLE
                 withStep:IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_1];
}

@end
