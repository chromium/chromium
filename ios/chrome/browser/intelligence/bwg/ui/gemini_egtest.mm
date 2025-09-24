// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/scoped_feature_list.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Matcher for the primary button in the promo view.
id<GREYMatcher> PromoPrimaryButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_BWG_PROMO_PRIMARY_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the secondary button in the promo view.
id<GREYMatcher> PromoSecondaryButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_BWG_PROMO_SECONDARY_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the primary button in the consent view.
id<GREYMatcher> ConsentPrimaryButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the secondary button in the consent view.
id<GREYMatcher> ConsentSecondaryButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the Gemini button.
id<GREYMatcher> GeminiButton() {
  return grey_allOf(grey_accessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_AI_HUB_GEMINI_LABEL)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

}  // namespace

// Test suite for BWG UI.
@interface GeminiEGTest : ChromeTestCase
@end

@implementation GeminiEGTest

- (void)setUp {
  [super setUp];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyAppInterface
       addFakeIdentity:fakeIdentity
      withCapabilities:@{@(kCanUseModelExecutionFeaturesName) : @YES}];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kPageActionMenu);
  return config;
}

// Tests that the FRE is displayed correctly from the Page Action Menu.
- (void)testFREFromPageActionMenu {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAIHubEntrypointAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap the Gemini button.
  [[EarlGrey selectElementWithMatcher:GeminiButton()] performAction:grey_tap()];

  // Check that the promo buttons are visible.
  [[EarlGrey selectElementWithMatcher:PromoPrimaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PromoSecondaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the primary button to advance to the consent screen.
  [[EarlGrey selectElementWithMatcher:PromoPrimaryButton()]
      performAction:grey_tap()];

  // Check that the consent buttons are visible.
  [[EarlGrey selectElementWithMatcher:ConsentPrimaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ConsentSecondaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
