// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/scoped_feature_list.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_traits_overrider.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Constant displacement in points for scrolling the carousel by one slide.
constexpr CGFloat kCarouselScrollDisplacement = 250;

// Matcher for the primary button in the promo view.
id<GREYMatcher> PromoPrimaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the secondary button in the promo view.
id<GREYMatcher> PromoSecondaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackSecondaryActionAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the primary button in the consent view.
id<GREYMatcher> ConsentPrimaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the secondary button in the consent view.
id<GREYMatcher> ConsentSecondaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackSecondaryActionAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the Gemini button.
id<GREYMatcher> GeminiButton() {
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    if ([ChromeEarlGrey isIPadIdiom] && ![ChromeEarlGrey isCompactWidth]) {
      return grey_allOf(grey_accessibilityID(kToolbarAssistantButtonIdentifier),
                        grey_accessibilityTrait(UIAccessibilityTraitButton),
                        nil);
    }
    return grey_allOf(grey_accessibilityID(kAppBarAssistantButtonId),
                      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
  }
  return grey_allOf(
      grey_accessibilityID(kAIHubAskGeminiButtonAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for slide titles in the visual-rich carousel.
id<GREYMatcher> VisualRichSlideTitle(int string_id) {
  NSString* baseTitle = l10n_util::GetNSString(string_id);
  NSString* suffix =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_CAROUSEL_GEMINI_IN_CHROME);
  NSString* expectedTitle =
      (string_id == IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE)
          ? [NSString stringWithFormat:@"%@\n%@", baseTitle, suffix]
          : [NSString stringWithFormat:@"%@ %@", baseTitle, suffix];
  return grey_allOf(grey_accessibilityLabel(expectedTitle),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the UIPageControl in visual-rich carousel view.
id<GREYMatcher> VisualRichPageControl() {
  return grey_allOf(grey_kindOfClass([UIPageControl class]),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for any sufficiently visible slide title in the carousel.
id<GREYMatcher> AnyVisualRichSlideTitle() {
  return grey_anyOf(
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE),
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_TITLE),
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE), nil);
}

// Matcher for the scroll view in the visual-rich carousel.
id<GREYMatcher> VisualRichCarouselScrollView() {
  return grey_allOf(
      grey_accessibilityID(kGeminiFRECarouselScrollViewAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the title in the lightweight FRE.
id<GREYMatcher> LightweightFRETitle(int string_id) {
  NSString* expectedTitle = l10n_util::GetNSString(string_id);
  return grey_allOf(grey_accessibilityLabel(expectedTitle),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the first consent bullet point title.
id<GREYMatcher> FirstConsentBulletPointTitle() {
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE);
  return grey_text(expectedTitle);
}

}  // namespace

// Test suite for BWG UI.
@interface GeminiEGTest : ChromeTestCase
@end

@implementation GeminiEGTest

- (void)setUp {
  [super setUp];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyAppInterface addFakeIdentity:fakeIdentity
                             withCapabilities:@{
                               @(kCanUseModelExecutionFeaturesName) : @YES,
                               @(kCanUseGeminiInChromeCapabilityName) : @YES
                             }];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kAIHubEligibilityTriggered];
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kIOSBwgConsent];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kPageActionMenu);

  if ([self isRunningTest:@selector(testAIHubNewBadgeAccessibility)]) {
    config.iph_feature_enabled = "IPH_iOSAIHubNewBadge";
    config.relaunch_policy = ForceRelaunchByKilling;
  }
  [self configureFirstRunExperimentForConfiguration:config];

  return config;
}

// Configures First Run Experiment feature flags and parameters based on the
// active test case.
- (void)configureFirstRunExperimentForConfiguration:
    (AppLaunchConfiguration&)config {
  std::string experimentParam;
  if ([self isRunningTest:@selector(
                              testLightweightConvenienceFREAcceptConsent)] ||
      [self isRunningTest:@selector(
                              testLightweightConvenienceFREDeclineConsent)] ||
      [self
          isRunningTest:
              @selector(testLightweightConvenienceFREMaxContentSizeCategory)] ||
      [self
          isRunningTest:@selector(
                            testLightweightConvenienceFREOrientationChange)]) {
    experimentParam = kGeminiFREExperimentParamLightweightConvenience;
  } else if ([self isRunningTest:
                       @selector(testLightweightPageSharingFRETitleVisible)]) {
    experimentParam = kGeminiFREExperimentParamLightweightPageSharing;
  } else if ([self isRunningTest:@selector(
                                     testLightweightDiverseFRETitleVisible)]) {
    experimentParam = kGeminiFREExperimentParamLightweightDiverse;
  } else if ([self isRunningTest:@selector(
                                     testVisualRichFRECarouselNavigation)] ||
             [self isRunningTest:@selector(testVisualRichFREAcceptConsent)] ||
             [self isRunningTest:@selector(testVisualRichFREDeclineConsent)] ||
             [self
                 isRunningTest:@selector(testVisualRichFREOrientationChange)] ||
             [self
                 isRunningTest:@selector(
                                   testVisualRichFREMaxContentSizeCategory)]) {
    experimentParam = kGeminiFREExperimentParamVisualRich;
  }

  if (!experimentParam.empty()) {
    config.features_enabled.push_back(kGeminiFRERefactor);
    config.features_enabled_and_params.push_back(
        {kGeminiFREExperiment, {{kGeminiFREExperimentParam, experimentParam}}});
    config.relaunch_policy = ForceRelaunchByKilling;
  } else {
    // Ensure these are disabled for the baseline First run UI tests.
    config.features_disabled.push_back(kGeminiFRERefactor);
    config.features_disabled.push_back(kGeminiFREExperiment);
  }
}

// Tests that when the promo is declined, the Gemini floaty is not presented.
- (void)testDeclinePromo {
  [self invokeGeminiEntryPoint];

  // Check that the promo buttons are visible.
  [[EarlGrey selectElementWithMatcher:PromoPrimaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the secondary button to decline the promo.
  [[EarlGrey selectElementWithMatcher:PromoSecondaryButton()]
      performAction:grey_tap()];

  // Verify the FRE is dismissed.
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:PromoPrimaryButton()];

  // Verify consent was not granted.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kIOSBwgConsent],
                  @"Consent should be false.");
}

// Tests that when the promo is accepted but consent is declined, the Gemini
// floaty is not presented.
- (void)testAcceptPromoDeclineConsent {
  [self invokeGeminiEntryPoint];

  // Tap the primary button to advance to the consent screen.
  [[EarlGrey selectElementWithMatcher:PromoPrimaryButton()]
      performAction:grey_tap()];

  // Wait for the consent screen to appear.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kGeminiFootNoteTextViewAccessibilityIdentifier)];

  [self declineConsentAndVerifyDismissal];
}

// Tests that when the promo and consent are both accepted, the Gemini floaty is
// presented.
- (void)testAcceptPromoAndConsent {
  [self invokeGeminiEntryPoint];

  // Tap the primary button to advance to the consent screen.
  [[EarlGrey selectElementWithMatcher:PromoPrimaryButton()]
      performAction:grey_tap()];

  // Wait for the consent screen to appear.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kGeminiFootNoteTextViewAccessibilityIdentifier)];

  [self acceptConsentAndVerifyDismissal];
}

// Tests that the AI Hub entry point conveys the "New" context to accessibility.
- (void)testAIHubNewBadgeAccessibility {
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    EARL_GREY_TEST_DISABLED(@"No 'new' label with Next");
  }
  NSString* baseLabel = l10n_util::GetNSString(
      IDS_IOS_BWG_PAGE_ACTION_MENU_ENTRY_POINT_ACCESSIBILITY_LABEL);
  NSString* expectedLabel =
      [NSString stringWithFormat:@"%@, %@", baseLabel,
                                 l10n_util::GetNSString(
                                     IDS_IOS_NEW_FEATURE_ACCESSIBILITY_HINT)];

  id<GREYMatcher> entrypointMatcher = grey_allOf(
      grey_accessibilityLabel(expectedLabel), grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:entrypointMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests manual user navigation on the Visual-Rich FRE carousel.
- (void)testVisualRichFRECarouselNavigation {
  [self invokeGeminiEntryPoint];

  // Verify initial slide 0 title and page control are visible.
  [[EarlGrey
      selectElementWithMatcher:VisualRichSlideTitle(
                                   IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:VisualRichPageControl()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe right on the first slide to verify it stays on slide 0.
  [[EarlGrey
      selectElementWithMatcher:VisualRichSlideTitle(
                                   IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE)]
      performAction:grey_swipeFastInDirection(kGREYDirectionRight)];

  // Verify slide 0 title is still visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE)];

  // Tap the page control to advance to slide 1.
  [[EarlGrey selectElementWithMatcher:VisualRichPageControl()]
      performAction:grey_tap()];

  // Verify slide 1 title is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_TITLE)];

  // Advance to slide 2.
  [self scrollCarouselNext];

  // Verify slide 2 title is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE)];

  // Swipe left on the last slide to verify it stays on slide 2.
  [[EarlGrey
      selectElementWithMatcher:VisualRichSlideTitle(
                                   IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE)]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify slide 2 title is still visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE)];

  // Return to slide 1.
  [self scrollCarouselPrevious];

  // Verify slide 1 title is visible again.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_TITLE)];
}

// Tests accepting consent from the Visual-Rich FRE screen.
- (void)testVisualRichFREAcceptConsent {
  [self invokeGeminiEntryPoint];
  [self acceptConsentAndVerifyDismissal];
}

// Tests declining consent from the Visual-Rich FRE screen.
- (void)testVisualRichFREDeclineConsent {
  [self invokeGeminiEntryPoint];
  [self declineConsentAndVerifyDismissal];
}

// Tests landscape orientation layout adaptation for Visual-Rich FRE.
- (void)testVisualRichFREOrientationChange {
  [self invokeGeminiEntryPoint];
  [self verifyOrientationChangeForTitleMatcher:AnyVisualRichSlideTitle()];
}

// Tests that under the maximum dynamic type size, the first consent bullet
// point remains visible in the Visual-Rich FRE.
- (void)testVisualRichFREMaxContentSizeCategory {
  [self invokeGeminiEntryPoint];
  [self verifyMaxContentSizeCategoryFirstBulletPoint];
}

// Tests accepting consent from the Lightweight Convenience FRE screen.
- (void)testLightweightConvenienceFREAcceptConsent {
  [self invokeGeminiEntryPoint];

  // Verify the convenience title is visible.
  [[EarlGrey
      selectElementWithMatcher:
          LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self acceptConsentAndVerifyDismissal];
}

// Tests declining consent from the Lightweight Convenience FRE screen.
- (void)testLightweightConvenienceFREDeclineConsent {
  [self invokeGeminiEntryPoint];

  // Verify the convenience title is visible.
  [[EarlGrey
      selectElementWithMatcher:
          LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self declineConsentAndVerifyDismissal];
}

// Tests landscape orientation layout adaptation for Lightweight Convenience
// FRE.
- (void)testLightweightConvenienceFREOrientationChange {
  [self invokeGeminiEntryPoint];
  [self
      verifyOrientationChangeForTitleMatcher:
          LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE)];
}

// Tests that under the maximum dynamic type size, the first consent bullet
// point remains visible in the Lightweight FRE.
- (void)testLightweightConvenienceFREMaxContentSizeCategory {
  [self invokeGeminiEntryPoint];
  [self verifyMaxContentSizeCategoryFirstBulletPoint];
}

// Tests that the Lightweight Page Sharing FRE screen displays the expected
// title.
- (void)testLightweightPageSharingFRETitleVisible {
  [self invokeGeminiEntryPoint];

  // Verify the page sharing title is visible.
  [[EarlGrey
      selectElementWithMatcher:
          LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_PAGE_SHARING_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Lightweight Diverse FRE screen displays the expected title.
- (void)testLightweightDiverseFRETitleVisible {
  [self invokeGeminiEntryPoint];

  // Verify the diverse title is visible.
  [[EarlGrey
      selectElementWithMatcher:LightweightFRETitle(
                                   IDS_IOS_BWG_LIGHTWEIGHT_PROMO_DIVERSE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Helpers

- (void)invokeGeminiEntryPoint {
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    // With ChromeNext, the entry point is directly the Gemini button in the
    // AppBar.
    [[EarlGrey selectElementWithMatcher:GeminiButton()]
        performAction:grey_tap()];
  } else {
    id<GREYMatcher> entrypointMatcher = grey_allOf(
        grey_accessibilityID(kAIHubEntrypointAccessibilityIdentifier),
        grey_sufficientlyVisible(), nil);

    [[EarlGrey selectElementWithMatcher:entrypointMatcher]
        performAction:grey_tap()];

    // Tap the Gemini button.
    [[EarlGrey selectElementWithMatcher:GeminiButton()]
        performAction:grey_tap()];
  }

  // Wait for the FRE screen to appear.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ConsentPrimaryButton()];
}

// Taps the primary button to grant consent and verifies dismissal and pref.
- (void)acceptConsentAndVerifyDismissal {
  [[EarlGrey selectElementWithMatcher:ConsentPrimaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ConsentPrimaryButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:ConsentPrimaryButton()];

  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kIOSBwgConsent],
                 @"Consent should be true.");
}

// Taps the secondary button to decline consent and verifies dismissal and pref.
- (void)declineConsentAndVerifyDismissal {
  [[EarlGrey selectElementWithMatcher:ConsentSecondaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ConsentSecondaryButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:ConsentSecondaryButton()];

  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kIOSBwgConsent],
                  @"Consent should be false.");
}

// Verifies that a title and the consent button remain visible across portrait
// and landscape orientations.
- (void)verifyOrientationChangeForTitleMatcher:(id<GREYMatcher>)titleMatcher {
  // Verify title and consent primary button are visible in portrait.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:titleMatcher];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ConsentPrimaryButton()];

  // Rotate device interface to landscape.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];

  // Verify title and consent primary button remain visible in landscape.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:titleMatcher];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ConsentPrimaryButton()];

  // Rotate back to portrait.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];

  // Verify title and consent primary button remain visible in portrait.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:titleMatcher];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ConsentPrimaryButton()];
}

// Overrides dynamic type size to max accessibility category and verifies the
// first consent bullet point is visible.
- (void)verifyMaxContentSizeCategoryFirstBulletPoint {
  ScopedTraitOverrider overrider([self topPresentedViewController]);
  overrider.SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      FirstConsentBulletPointTitle()];
}

// Scrolls the visual-rich carousel to the next slide (right).
- (void)scrollCarouselNext {
  [[EarlGrey selectElementWithMatcher:VisualRichCarouselScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionRight,
                                           kCarouselScrollDisplacement)];
}

// Scrolls the visual-rich carousel to the previous slide (left).
- (void)scrollCarouselPrevious {
  [[EarlGrey selectElementWithMatcher:VisualRichCarouselScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionLeft,
                                           kCarouselScrollDisplacement)];
}

// Returns the top presented view controller.
- (UIViewController*)topPresentedViewController {
  UIViewController* topController =
      chrome_test_util::GetAnyKeyWindow().rootViewController;
  while (topController.presentedViewController &&
         !topController.presentedViewController.isBeingDismissed) {
    topController = topController.presentedViewController;
  }
  return topController;
}

@end
