// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
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
  [ChromeEarlGrey waitForAndTapButton:item];
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
  if ([button waitForExistenceWithTimeout:1]) {
    // Wait for the magic stack to settle behind the alert.
    // Otherwise the test flakes when a snackbar is presented right after the
    // permissions alert is dismissed.
    [ChromeEarlGreyUI waitForAppToIdle];
    [button tap];
  }
}

// Taps an iOS notification.
void TapNotification() {
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  auto notification =
      springboardApplication.otherElements[@"Notification"].firstMatch;
  BOOL notificationAppeared = [notification
      waitForExistenceWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                      .InSecondsF()];
  [notification tap];
  XCTAssert(notificationAppeared, @"A notification did not appear");
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

  std::string triggerTime = "2.5s";

  if ([self isRunningTest:@selector(testToggleTipsNotificationsMenuItem)]) {
    triggerTime = "72h";
  }

  // Enable Tips Notifications with trigger time params.
  std::string enableFeatures = base::StringPrintf(
      "--enable-features=%s:%s/%s/%s/%s/%s/%s", kIOSTipsNotifications.name,
      kIOSTipsNotificationsUnknownTriggerTimeParam, triggerTime.c_str(),
      kIOSTipsNotificationsLessEngagedTriggerTimeParam, triggerTime.c_str(),
      kIOSTipsNotificationsActiveSeekerTriggerTimeParam, triggerTime.c_str());
  config.additional_args.push_back(enableFeatures);
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ChromeEarlGrey writeFirstRunSentinel];
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIosDefaultBrowserPromoLastAction];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
}

- (void)tearDown {
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [super tearDown];
}

#pragma mark - Helpers

// Opt in to Tips Notications via the SetUpList long-press menu. Mark all
// Tips Notifications as "sent", except for the ones included in `types`.
- (void)optInToTipsNotifications:(std::vector<TipsNotificationType>)types {
  // Long press the SetUpList module.
  id<GREYMatcher> setUpList =
      grey_accessibilityID(set_up_list::kDefaultBrowserItemID);
  [[EarlGrey selectElementWithMatcher:setUpList]
      performAction:grey_longPress()];

  // This uses kTipsNotificationsSentPref to mark all notifications except for
  // the ones listed in `types` as "sent", which will ensure that they are not
  // sent again during the current test case.
  int sentBits = 0xffff;
  for (auto type : types) {
    sentBits ^= 1 << int(type);
  }
  [ChromeEarlGrey setIntegerValue:sentBits
                forLocalStatePref:kTipsNotificationsSentPref];

  // Tap the menu item to enable notifications.
  TapText(@"Turn on Notifications");
  MaybeTapAllowNotifications();

  // Tap the confirmation snackbar.
  WaitForThenTapText(@"notifications turned on");
}

// Turn off Tips Notifications via the SetUpList long-press menu.
- (void)turnOffTipsNotifications {
  // Long press the SetUpList module.
  id<GREYMatcher> setUpList =
      grey_accessibilityID(set_up_list::kDefaultBrowserItemID);
  [[EarlGrey selectElementWithMatcher:setUpList]
      performAction:grey_longPress()];

  // Tap the menu item to enable notifications.
  TapText(@"Turn off Notifications");

  // Tap the confirmation snackbar.
  WaitForThenTapText(@"notifications turned off");
}

#pragma mark - Tests

// Tests the SetUpList long press menu item to toggle Tips Notifications.
- (void)testToggleTipsNotificationsMenuItem {
  [self optInToTipsNotifications:{}];
  [self turnOffTipsNotifications];
}

// Tests triggering and interacting with each of the Tips notifications.
- (void)testTriggerNotifications {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGreyUI waitForAppToIdle];

  MaybeDismissNotification();

  [self optInToTipsNotifications:{
                                     TipsNotificationType::kWhatsNew,
                                     TipsNotificationType::kOmniboxPosition,
                                     TipsNotificationType::kDefaultBrowser,
                                     TipsNotificationType::kDocking,
                                     TipsNotificationType::kSignin,
                                 }];

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

  // OmniboxPositionChoice is only available on phones.
  if ([ChromeEarlGrey isIPhoneIdiom]) {
    // Wait for and tap the Omnibox Position notification.
    TapNotification();

    // Verify that the Omnibox Position view is showing.
    id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
        first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

    // Dismiss the Omnibox Position view.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::PromoStyleSecondaryActionButtonMatcher()]
        performAction:grey_tap()];
  }

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

  // Wait for and tap the Docking promo notification.
  TapNotification();

  // Verify the Docking promo is showing.
  id<GREYMatcher> dockingPromoView =
      grey_accessibilityID(@"kDockingPromoAccessibilityId");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dockingPromoView];

  // Tap "Got It" on the Docking promo view.
  id<GREYMatcher> gotItButton = grey_accessibilityID(
      kConfirmationAlertPrimaryActionAccessibilityIdentifier);
  [ChromeEarlGrey waitForAndTapButton:gotItButton];

  // Wait for and tap the Signin notification.
  TapNotification();

  // Verify the signin screen is showing.
  id<GREYMatcher> signinView =
      grey_accessibilityID(kWebSigninAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:signinView];

  // Dismiss Signin.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];
}

@end
