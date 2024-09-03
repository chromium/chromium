// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns the matcher for the Tips Notifications switch.
id<GREYMatcher> TipsSwitchMatcher() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TIPS_TITLE);
  return grey_accessibilityID([NSString stringWithFormat:@"%@, switch", title]);
}

// Returns the matcher for the Tips Notifications switch.
id<GREYMatcher> ContentSwitchMatcher() {
  return grey_accessibilityID(@"Personalized Content, switch");
}

// Returns the matcher for the Notifications Opt-In Screen.
id<GREYMatcher> OptInScreenMatcher() {
  return grey_accessibilityID(@"NotificationsOptInScreenAxId");
}

}  // namespace

// Test suite that tests user interactions with the Notifications Opt-In Screen.
@interface NotificationsOptInTestCase : ChromeTestCase
@end

@implementation NotificationsOptInTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSTipsNotifications);
  config.features_enabled.push_back(kContentPushNotifications);
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ChromeEarlGrey writeFirstRunSentinel];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [super tearDown];
}

// Triggers the Notifications Opt-In Screen through the Set Up List "See more"
// view.
- (void)triggerOptInScreen {
  // Open the "See more" view.
  id seeMoreButton =
      grey_allOf(grey_text(@"See more"), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:seeMoreButton] performAction:grey_tap()];

  // Swipe up to expand the "See more" view.
  id setUpListSubtitle = chrome_test_util::ContainsPartialText(
      @"Complete these suggested actions below");
  [[EarlGrey selectElementWithMatcher:setUpListSubtitle]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Tap on the "Try" button for the Notifications item.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"Get notifications Try Button")]
      performAction:grey_tap()];

  // Verify the opt-in screen is showing.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:OptInScreenMatcher()];
}

// Asserts that the primary action button is enabled (or not).
- (void)assertPrimaryButtonEnabled:(BOOL)enabled {
  id<GREYMatcher> primaryButton =
      grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
  id<GREYMatcher> enabledMatcher =
      enabled ? grey_enabled() : grey_not(grey_enabled());
  [[EarlGrey selectElementWithMatcher:primaryButton]
      assertWithMatcher:enabledMatcher];
}

// Tests that the primary action button is disabled initially and enabled after
// a toggle is switched on.
- (void)testToggleEnablesButton {
  [ChromeEarlGreyUI waitForAppToIdle];
  [self triggerOptInScreen];

  [self assertPrimaryButtonEnabled:NO];

  // Toggle on the switch.
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      performAction:grey_turnSwitchOn(YES)];

  [self assertPrimaryButtonEnabled:YES];

  // Toggle off the switch.
  [[EarlGrey selectElementWithMatcher:TipsSwitchMatcher()]
      performAction:grey_turnSwitchOn(NO)];

  [self assertPrimaryButtonEnabled:NO];

  // Dismiss prompt by tapping on secondary action button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPromoStyleSecondaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OptInScreenMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that Content Notificaion item is not shown without user eligibility
// fulfilled and feature flag enalbed.
- (void)testContentNotificationItemNotShow {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGreyUI waitForAppToIdle];
  [self triggerOptInScreen];

  // The Content Notification item should not be shown at this time, becasue
  // feature flag and user eligibility is not fulfilled by default setup.
  [[EarlGrey selectElementWithMatcher:ContentSwitchMatcher()]
      assertWithMatcher:grey_nil()];
}

@end
