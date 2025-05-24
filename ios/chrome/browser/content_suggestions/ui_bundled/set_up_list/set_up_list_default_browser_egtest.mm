// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Returns a matcher to the Default Browser Set Up List item default
// description.
id<GREYMatcher> DefaultItemDescription() {
  return grey_text(
      GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SHORT_DESCRIPTION));
}

// Returns a matcher to the Default Browser see more Set Up List item default
// title.
id<GREYMatcher> DefaultItemSeeMoreTitle() {
  return grey_allOf(
      grey_text(GetNSString([ChromeEarlGrey isIPadIdiom]
                                ? IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE_IPAD
                                : IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE)),
      grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Default Browser see more Set Up List item default
// description.
id<GREYMatcher> DefaultItemSeeMoreDescription() {
  return grey_allOf(
      grey_text(GetNSString(
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SEE_MORE_DESCRIPTION)),
      grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Default Browser Set Up List item default title.
id<GREYMatcher> DefaultItemTitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE_IPAD
                      : IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE));
}

// Returns a matcher to the Set Up List Default Browser promo default title.
id<GREYMatcher> DefaultPromoTitle() {
  return grey_allOf(
      grey_text(
          GetNSString([ChromeEarlGrey isIPadIdiom]
                          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)),
      grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo default subtitle.
id<GREYMatcher> DefaultPromoSubtitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
}

}  // namespace

// Tests the Default Browser item in the Set Up List.
@interface SetUpListDefaultBrowserTestCase : ChromeTestCase
@end

@implementation SetUpListDefaultBrowserTestCase

#pragma mark - BaseEarlGreyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Set first run details to show Set Up List.
  config.additional_args.push_back("-FirstRunRecency");
  config.additional_args.push_back("1");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  return config;
}

- (void)setUp {
  [super setUp];
  [self prepareToTestSetUpListInMagicStack];
}

#pragma mark - Tests

// Tests that the default text on the Default Browser compacted Set Up List item
// is correctly displayed.
- (void)testDefaultCompactedItem {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultItemDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the default text on the Default Browser see more Set Up List item
// is correctly displayed.
- (void)testDefaultSeeMoreItem {
  [self openSeeMore];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the default text on the Set Up List Default Browser promo is
// correctly displayed.
- (void)testDefaultPromo {
  [self openPromo];
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Helpers

// Opens the Set Up List "See More" view.
- (void)openSeeMore {
  id<GREYMatcher> seeMoreButton =
      grey_allOf(grey_text(@"See more"), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:seeMoreButton] performAction:grey_tap()];
  // Swipe up to expand the "See more" view.
  id<GREYMatcher> setUpListSubtitle = chrome_test_util::ContainsPartialText(
      @"Complete these suggested actions below");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:setUpListSubtitle];
  if (![ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:setUpListSubtitle]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  }
}

- (void)openPromo {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];
}

// Sets conditions so that the Set Up List will display with no completed items.
- (void)prepareToTestSetUpListInMagicStack {
  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

@end
