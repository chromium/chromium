// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
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

namespace {

/// Label of the top address bar option.
NSString* TopAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_TOP_ADDRESS_BAR_OPTION);
}

/// Label of the bottom address bar option.
NSString* BottomAddressBarLabelText() {
  return l10n_util::GetNSString(IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION);
}

/// Matcher for the top address bar option.
id<GREYMatcher> TopAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      TopAddressBarLabelText());
}

/// Matcher for the bottom address bar option.
id<GREYMatcher> BottomAddressBarOption() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      BottomAddressBarLabelText());
}

/// Matcher for the top address bar option when selected.
id<GREYMatcher> TopAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), TopAddressBarOption(), nil);
}

/// Matcher for the bottom address bar option when selected.
id<GREYMatcher> BottomAddressBarOptionSelected() {
  return grey_allOf(grey_selected(), BottomAddressBarOption(), nil);
}

/// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
/// scroll.
void TapPromoStyleButton(NSString* buttonIdentifier) {
  id<GREYMatcher> buttonMatcher = grey_accessibilityID(buttonIdentifier);
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

/// Skip the FRE screens before omnibox position choice.
void SkipScreensBeforeOmniboxPositionChoice() {
  // Skip sign-in.
  TapPromoStyleButton(kPromoStyleSecondaryActionAccessibilityIdentifier);
  // Skip default browser.
  TapPromoStyleButton(kPromoStyleSecondaryActionAccessibilityIdentifier);
}

}  // namespace

#pragma mark - FRE promo

/// Tests the omnibox position choice screen in FRE promo.
@interface OmniboxPositionFirstRunTestCase : ChromeTestCase
@end

@implementation OmniboxPositionFirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kBottomOmnibox];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Disable the search engine choice at the end of FRE.
  // TODO(b/289998773): Re-enable it. Update EG test so that they
  // close this view if they need to interact more after the FRE.
  config.additional_args.push_back(std::string("--") +
                                   switches::kDisableSearchEngineChoiceScreen);
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  if ([self isRunningTest:@selector(testSelectTopWithTopDefault)] ||
      [self isRunningTest:@selector(testSelectBottomWithTopDefault)]) {
    std::string top_option_by_default =
        std::string(kBottomOmniboxPromoDefaultPosition.name) + ":" +
        kBottomOmniboxPromoDefaultPositionParam + "/" +
        kBottomOmniboxPromoDefaultPositionParamTop;

    config.additional_args.push_back(
        "--enable-features=" + top_option_by_default + "," +
        kBottomOmniboxPromoFRE.name);
  } else if ([self
                 isRunningTest:@selector(testSelectBottomWithBottomDefault)] ||
             [self isRunningTest:@selector(testSelectTopWithBottomDefault)]) {
    std::string bottom_option_by_default =
        std::string(kBottomOmniboxPromoDefaultPosition.name) + ":" +
        kBottomOmniboxPromoDefaultPositionParam + "/" +
        kBottomOmniboxPromoDefaultPositionParamBottom;

    config.additional_args.push_back(
        "--enable-features=" + bottom_option_by_default + "," +
        kBottomOmniboxPromoFRE.name);
  }
  return config;
}

#pragma mark Tests

// Tests selecting top omnibox in FRE when top is selected by default.
- (void)testSelectTopWithTopDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  SkipScreensBeforeOmniboxPositionChoice();

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is top.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kBottomOmnibox],
                  @"Failed to set preferred omnibox position to top");
}

// Tests selecting bottom omnibox in FRE when top is selected by default.
- (void)testSelectBottomWithTopDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  SkipScreensBeforeOmniboxPositionChoice();

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Tap on the bottom address bar option.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOption()]
      performAction:grey_tap()];

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is bottom.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kBottomOmnibox],
                 @"Failed to set preferred omnibox position to bottom");
}

// Tests selecting bottom omnibox in FRE when bottom is selected by default.
- (void)testSelectBottomWithBottomDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  SkipScreensBeforeOmniboxPositionChoice();

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is bottom.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kBottomOmnibox],
                 @"Failed to set preferred omnibox position to bottom");
}

// Tests selecting top omnibox in FRE when bottom is selected by default.
- (void)testSelectTopWithBottomDefault {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  SkipScreensBeforeOmniboxPositionChoice();

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Tap on the top address bar option.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOption()]
      performAction:grey_tap()];

  // The top address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:TopAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is top.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kBottomOmnibox],
                  @"Failed to set preferred omnibox position to top");
}

@end

#pragma mark - App-launch promo

/// The the omnibox position choice screen in app-launch promo.
@interface OmniboxPositionAppLaunchTestCase : ChromeTestCase
@end

@implementation OmniboxPositionAppLaunchTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kBottomOmnibox];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back("-enable-promo-manager-fullscreen-promos");
  // Override trigger requirements to force the promo to appear.
  config.additional_args.push_back("-NextPromoForDisplayOverride");
  config.additional_args.push_back("promos_manager::Promo::OmniboxPosition");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Disable FET in promo manager as the initialization takes too much time and
  // causes the test to fail (crbug.com/1505431).
  config.features_disabled.push_back(kPromosManagerUsesFET);

  std::string bottomOptionByDefault =
      std::string(kBottomOmniboxPromoDefaultPosition.name) + ":" +
      kBottomOmniboxPromoDefaultPositionParam + "/" +
      kBottomOmniboxPromoDefaultPositionParamBottom;

  config.additional_args.push_back(
      "--enable-features=" + bottomOptionByDefault + "," +
      kBottomOmniboxPromoAppLaunch.name);

  return config;
}

#pragma mark Tests

/// Tests confirming the default omnibox option in app-launch promo.
- (void)testConfirmDefaultOption {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Confirm selection.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);

  // Verify that the preferred omnibox position is bottom.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kBottomOmnibox],
                 @"Failed to set preferred omnibox position to bottom");
}

/// Tests discarding the omnibox position choice app-launch promo.
- (void)testNoThanks {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no choice for omnibox position on tablet)");
  }

  // The bottom address bar option should be selected.
  [[EarlGrey selectElementWithMatcher:BottomAddressBarOptionSelected()]
      assertWithMatcher:grey_notNil()];

  // Discard selection.
  TapPromoStyleButton(kPromoStyleSecondaryActionAccessibilityIdentifier);

  // Verify that there is no user preferred omnibox position.
  GREYAssertTrue(
      [ChromeEarlGrey prefWithNameIsDefaultValue:prefs::kBottomOmnibox],
      @"Failed to discard the selected position");
}

@end
