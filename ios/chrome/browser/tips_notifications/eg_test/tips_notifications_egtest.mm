// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "base/threading/platform_thread.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Wait for a view that contains a partial match to the given `text`, then tap
// it.
void WaitForThenTapText(NSString* text) {
  id item = chrome_test_util::ContainsPartialText(text);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:item];
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Taps a view containing a partial match to the given `text`.
void TapText(NSString* text) {
  id item = grey_allOf(chrome_test_util::ContainsPartialText(text),
                       grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_tap()];
}

// Taps "Allow" on notification permissions alert, if it appears.
void MaybeTapAllowNotifications() {
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  auto button = springboardApplication.buttons[@"Allow"];
  if ([button waitForExistenceWithTimeout:2]) {
    [button tap];
    [ChromeEarlGreyUI waitForAppToIdle];
  }
}

// Taps an iOS notification.
void TapNotification() {
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  auto notification =
      springboardApplication.otherElements[@"Notification"].firstMatch;
  GREYAssert([notification waitForExistenceWithTimeout:4],
             @"A notification did not appear");
  [notification tap];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Dismiss a notification, if one exists.
void MaybeDismissNotification() {
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  auto notification =
      springboardApplication.otherElements[@"Notification"].firstMatch;
  if ([notification waitForExistenceWithTimeout:2]) {
    [notification swipeUp];
  }
}

}  // namespace

// Test case for Tips Notifications.
@interface TipsNotificationsTestCase : ChromeTestCase
@end

@implementation TipsNotificationsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  //  config.features_enabled.push_back(kIOSTipsNotifications);

  // Enable Tips Notifications with 1s trigger time.
  std::string enableFeatures = base::StringPrintf(
      "--enable-features=%s:%s/%s", kIOSTipsNotifications.name,
      kIOSTipsNotificationsTriggerTimeParam, "2.5s");
  config.additional_args.push_back(enableFeatures);
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];

  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [super tearDown];
}

- (void)optInToTipsNotifications {
  // Long press the SetUpList module.
  id<GREYMatcher> setUpList =
      grey_accessibilityID(set_up_list::kDefaultBrowserItemID);
  [[EarlGrey selectElementWithMatcher:setUpList]
      performAction:grey_longPress()];

  // Clear pref that stores which notifications have been sent.
  [ChromeEarlGrey resetDataForLocalStatePref:kTipsNotificationsSentPref];

  // Tap the menu item to enable notifications.
  TapText(@"Turn on Notifications");
  MaybeTapAllowNotifications();

  // Tap the confirmation snackbar.
  WaitForThenTapText(@"notifications turned on");
}

// Tests triggering and interacting with each of the Tips notifications.
- (void)testTriggerNotifications {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGreyUI waitForAppToIdle];

  MaybeDismissNotification();

  [self optInToTipsNotifications];

  // Wait for and tap the Default Browser Notification.
  TapNotification();

  // Verify that the Default Browser Promo is visible.
  id<GREYMatcher> defaultBrowserView =
      chrome_test_util::DefaultBrowserSettingsTableViewMatcher();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:defaultBrowserView];

  // Tap "cancel".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Wait for and tap the What's New notification.
  TapNotification();

  // Verify that the What's New screen is showing.
  id<GREYMatcher> whatsNewView = grey_accessibilityID(@"kWhatsNewListViewId");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:whatsNewView];

  // Dismiss the What's New screen.
  id<GREYMatcher> whatsNewDoneButton =
      grey_accessibilityID(@"kWhatsNewTableViewNavigationDismissButtonId");
  [[EarlGrey selectElementWithMatcher:whatsNewDoneButton]
      performAction:grey_tap()];

  // Wait for and tap the Signin notification.
  TapNotification();

  // Verify the signin screen is showing
  id<GREYMatcher> signinView =
      grey_accessibilityID(kWebSigninAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:signinView];

  // Dismiss Signin.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];
}

@end
