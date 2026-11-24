// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Returns a matcher for the default promo title.
id<GREYMatcher> DefaultPromoTitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
}

// Returns a matcher for the default promo subtitle.
id<GREYMatcher> DefaultPromoSubtitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE));
}

}  // namespace

// Test Default Browser promo that appears during the First Run Experience.
@interface DefaultBrowserScreenTestCase : ChromeTestCase
@end

@implementation DefaultBrowserScreenTestCase

#pragma mark - BaseEarlGreyTestCase

+ (void)setUpForTestCase {
  // Leave this empty so that the FRE shows during the first test.
}

- (void)setUp {
  [super setUp];
  [self signIn];
}

- (void)tearDownHelper {
  [self reset];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Animated Default Browser introduces a new Default Browser screen with a
  // different ID.
  config.features_disabled.push_back(
      first_run::kAnimatedDefaultBrowserPromoInFRE);

  // GCRGemini crashes on nil identity in this sign-in setup.
  config.features_enabled.push_back(kGeminiKillSwitch);

  // Add a fake identity at startup so the Sign-In screen is included in the FRE
  // sequence.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kAddFakeIdentitiesAtStartup);

  if ([self isRunningAndroidSwitcherTest]) {
    const char* armParam =
        [self isRunningTest:@selector(testAndroidSwitcherHybridPromoDisplayed)]
            ? first_run::kAndroidSwitcherDefaultBrowserPromoInFREArmHybrid
            : first_run::
                  kAndroidSwitcherDefaultBrowserPromoInFREArmGoogleEcosystem;
    config.features_enabled_and_params.push_back(
        {first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
         {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
           first_run::
               kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal},
          {first_run::kAndroidSwitcherDefaultBrowserPromoInFREArmParam,
           armParam}}});
    if ([self isRunningTest:@selector(
                                testAndroidSwitcherLensFallbackDisplayed)]) {
      // Lens is unavailable in public builds and on iPad; disabling the Lens
      // camera also makes it unavailable in branded builds.
      config.features_enabled.push_back(kDisableLensCamera);
    }
    config.additional_args.push_back(
        "-force-device-switcher-experience=AndroidPhone");
  } else {
    config.features_disabled.push_back(
        first_run::kAndroidSwitcherDefaultBrowserPromoInFRE);
  }

  return config;
}

#pragma mark - Helpers

// Returns whether the currently running test expects the Android Switcher
// Default Browser promo screen.
- (BOOL)isRunningAndroidSwitcherTest {
  return
      [self isRunningTest:@selector(testAndroidSwitcherPromoDisplayed)] ||
      [self isRunningTest:@selector(testAndroidSwitcherHybridPromoDisplayed)] ||
      [self isRunningTest:@selector(testAndroidSwitcherLensFallbackDisplayed)];
}

// Signs in and enables sync.
- (void)signIn {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunSignInScreenAccessibilityIdentifier)];
  // The startup identity may not have been cleared (e.g. for the first test of
  // the suite), so only add it if needed.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  if (![SigninEarlGrey isIdentityAdded:fakeIdentity]) {
    [SigninEarlGrey addFakeIdentity:fakeIdentity];
  }
  // Sign in.
  [self tapPromoButton:chrome_test_util::ButtonStackPrimaryButton()];
  // Enable history/tab sync.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  [self tapPromoButton:chrome_test_util::ButtonStackPrimaryButton()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  NSString* expectedScreenAccessibilityID =
      [self isRunningAndroidSwitcherTest]
          ? first_run::
                kFirstRunDefaultBrowserAndroidSwitcherScreenAccessibilityIdentifier
          : first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier;
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              expectedScreenAccessibilityID)];
}

// Signs out and clears sync data.
- (void)reset {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey clearFakeSyncServerData];
}

// Taps a promo button.
- (void)tapPromoButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kButtonStackScrollViewAccessibilityIdentifier);
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

// Verifies the promo title, subtitle, and feature rows, showing Lens when
// available and Safe Browsing otherwise.
- (void)verifyAndroidSwitcherPromoWithExpectedTitle:(NSString*)expectedTitle {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::
                  kFirstRunDefaultBrowserAndroidSwitcherScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(expectedTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_YOUTUBE_MAPS_ROW))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_DRIVE_PHOTOS_ROW))]
      assertWithMatcher:grey_sufficientlyVisible()];

  int expectedRowMessageID;
  int hiddenRowMessageID;
  if ([ChromeEarlGrey isLensAvailable]) {
    expectedRowMessageID =
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_LENS_ROW;
    hiddenRowMessageID =
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SAFE_BROWSING_ROW;
  } else {
    expectedRowMessageID =
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SAFE_BROWSING_ROW;
    hiddenRowMessageID =
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_LENS_ROW;
  }
  [[EarlGrey
      selectElementWithMatcher:grey_text(GetNSString(expectedRowMessageID))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_text(GetNSString(hiddenRowMessageID))]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Tests

// Tests if the default promo is correctly displayed.
- (void)testDefaultPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Google Ecosystem arm of the Android Switcher default browser
// promo is displayed when the flag is enabled and the user is signed in.
- (void)testAndroidSwitcherPromoDisplayed {
  NSString* expectedTitle = GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_GOOGLE_ECOSYSTEM_TITLE_IPAD
          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_GOOGLE_ECOSYSTEM_TITLE);
  [self verifyAndroidSwitcherPromoWithExpectedTitle:expectedTitle];
}

// Tests that the Hybrid arm of the Android Switcher default browser promo is
// displayed when selected and the user is signed in.
- (void)testAndroidSwitcherHybridPromoDisplayed {
  NSString* expectedTitle = GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_HYBRID_TITLE);
  [self verifyAndroidSwitcherPromoWithExpectedTitle:expectedTitle];
}

// Tests that the Android Switcher default browser promo shows the Safe Browsing
// row instead of the Lens row when Lens is unavailable.
- (void)testAndroidSwitcherLensFallbackDisplayed {
  GREYAssertFalse([ChromeEarlGrey isLensAvailable],
                  @"Lens should be unavailable with the Lens camera disabled.");
  NSString* expectedTitle = GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_GOOGLE_ECOSYSTEM_TITLE_IPAD
          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_GOOGLE_ECOSYSTEM_TITLE);
  [self verifyAndroidSwitcherPromoWithExpectedTitle:expectedTitle];
}

@end
