// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/push_notification/ui_bundled/scoped_notification_auth_swizzler.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

constexpr base::TimeDelta kWaitForNotificationTimeout = base::Seconds(10);

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

// Taps "Allow" on notification permissions popup or Camera popup, if
// either appears.
void MaybeTapAllowOnPopup() {
  [ChromeEarlGreyUI waitForAppToIdle];
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  // Wait for allow or ok button to appear.
  auto button = [springboardApplication.buttons elementBoundByIndex:1];
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
      waitForExistenceWithTimeout:kWaitForNotificationTimeout.InSecondsF()];
  if (notificationAppeared) {
    [notification tap];
  }
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

  if ([self isRunningTest:@selector(testReactivation)]) {
    config.features_enabled.push_back(kIOSReactivationNotifications);
  } else {
    config.features_disabled.push_back(kIOSReactivationNotifications);
  }

  if ([self isRunningTest:@selector(testNotificationMIM)]) {
    config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);
  }

  config.features_disabled.push_back(kIOSOneTimeDefaultBrowserNotification);

  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kIosDefaultBrowserPromoLastAction];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [ChromeEarlGrey openNewTab];
  if (![self isRunningTest:@selector(testToggleTipsNotificationsMenuItem)]) {
    [ChromeEarlGrey setUserDefaultsObject:@(3)
                                   forKey:@"TipsNotificationTrigger"];
  }
}

- (void)tearDownHelper {
  [ChromeEarlGrey removeUserDefaultsObjectForKey:@"TipsNotificationTrigger"];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAppLevelPushNotificationPermissions];
  [ChromeEarlGrey removeUserDefaultsObjectForKey:@"edoTestPort"];
  [super tearDownHelper];
}

#pragma mark - Helpers

// Opt in to Tips Notications via the SetUpList long-press menu. Mark all
// Tips Notifications as "sent", except for the ones included in `types`.
- (void)optInToTipsNotifications:(std::vector<TipsNotificationType>)types {
  // Long press the SetUpList module.
  id<GREYMatcher> setUpList =
      grey_accessibilityID(set_up_list::kSetUpListContainerID);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:setUpList];
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
  TapText(@"Turn on notifications");
  MaybeTapAllowOnPopup();

  // Tap the confirmation snackbar.
  WaitForThenTapText(@"notifications turned on");
}

// Turn off Tips Notifications via the SetUpList long-press menu.
- (void)turnOffTipsNotifications {
  // Long press the SetUpList module.
  id<GREYMatcher> setUpList =
      grey_accessibilityID(set_up_list::kSetUpListContainerID);
  [[EarlGrey selectElementWithMatcher:setUpList]
      performAction:grey_longPress()];

  // Tap the menu item to enable notifications.
  TapText(@"Turn off notifications");

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
// TODO(crbug.com/455768805): Test is flaky.
- (void)FLAKY_testTriggerNotifications {
  // TODO(crbug.com/455768805): Re-enable the test.
  if (@available(iOS 26.1, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.1.");
  }

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
  [ChromeEarlGreyUI waitForAppToIdle];

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
    [ChromeEarlGreyUI waitForAppToIdle];

    // Verify that the Omnibox Position view is showing.
    id<GREYMatcher> omniboxPositionView = grey_accessibilityID(
        first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier);
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:omniboxPositionView];

    [[EarlGrey selectElementWithMatcher:
                   grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                              grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  }

  // Wait for and tap the Default Browser Notification.
  TapNotification();
  [ChromeEarlGreyUI waitForAppToIdle];

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
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the Docking promo is showing.
  id<GREYMatcher> dockingPromoView =
      grey_accessibilityID(@"kDockingPromoAccessibilityId");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dockingPromoView];

  // Tap "Got It" on the Docking promo view.
  id<GREYMatcher> gotItButton = chrome_test_util::ButtonStackPrimaryButton();
  [ChromeEarlGrey waitForAndTapButton:gotItButton];

  // Wait for and tap the Signin notification.
  TapNotification();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the signin screen is showing.
  id<GREYMatcher> signinView =
      grey_accessibilityID(kConsistencySigninAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:signinView];

  // Dismiss Signin.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConsistencySigninSkipButtonMatcher()]
      performAction:grey_tap()];
}

// Tests that the Lens Promo appears when tapping on the Lens notification.
- (void)testLensNotification {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  MaybeDismissNotification();
  [ChromeEarlGreyUI waitForAppToIdle];
  [self optInToTipsNotifications:{}];

  // Request the notification and tap it.
  [ChromeEarlGrey requestTipsNotification:TipsNotificationType::kLens];
  TapNotification();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                          @"kLensPromoAXID")];
  // Tap "Show me how".
  // Use `grey_sufficientlyVisible()` to target the visible secondary button.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  id<GREYMatcher> instructions =
      grey_accessibilityID(@"kLensPromoInstructionsAXID");
  // Swipe down to dismiss the instructions.
  [[EarlGrey selectElementWithMatcher:instructions]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Request the notification a second time.
  [ChromeEarlGrey requestTipsNotification:TipsNotificationType::kLens];
  TapNotification();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                          @"kLensPromoAXID")];
  // Tap "Show me how" again.
  // Use `grey_sufficientlyVisible()` to target the visible secondary button.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Tap "Go To Lens".
  // Use `grey_sufficientlyVisible()` to target the visible primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::ButtonStackPrimaryButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  MaybeTapAllowOnPopup();
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"escape" flags:0];
}

// Tests that a Tips notification can be triggered and tapped when multiprofile
// is enabled.
- (void)testNotificationMIM {
  [self testLensNotification];
}

// Tests that the ESB Promo appears when tapping on the ESB notification.
- (void)testEnhancedSafeBrowsingNotification {
  MaybeDismissNotification();
  [ChromeEarlGreyUI waitForAppToIdle];
  [self optInToTipsNotifications:{}];

  // Request the notification and tap it.
  [ChromeEarlGrey
      requestTipsNotification:TipsNotificationType::kEnhancedSafeBrowsing];
  TapNotification();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(@"kEnhancedSafeBrowsingPromoAXID")];
  // Tap "Show me how".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
  id<GREYMatcher> instructions =
      grey_accessibilityID(@"kEnhancedSafeBrowsingPromoInstructionsAXID");
  // Swipe down to dismiss the instructions.
  [[EarlGrey selectElementWithMatcher:instructions]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Tap "Show me how" again.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
  // Use `grey_sufficientlyVisible()` to target the visible primary button on
  // the instructions view, avoiding conflicts with the underlying promo view.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::ButtonStackPrimaryButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Request the notification a second time.
  [ChromeEarlGrey
      requestTipsNotification:TipsNotificationType::kEnhancedSafeBrowsing];
  TapNotification();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(@"kEnhancedSafeBrowsingPromoAXID")];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that the app adds a Reactivation notification request.
- (void)testReactivation {
  ScopedNotificationAuthSwizzler auth(YES);
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kPushNotificationAuthorizationStatus];
  [ChromeEarlGrey
      resetDataForLocalStatePref:kReactivationNotificationsCanceledCount];
  __block BOOL notificationRequested = NO;
  auto requestBlock = ^(id center, UNNotificationRequest* request,
                        void (^completionHandler)(NSError* error)) {
    XCTAssert(IsTipsNotification(request),
              @"Requested notification was not recognized.");
    notificationRequested = YES;
    completionHandler(nil);
  };
  EarlGreyScopedBlockSwizzler addRequest(
      @"UNUserNotificationCenter",
      @"addNotificationRequest:withCompletionHandler:", requestBlock);

  // Backgrounding and re-foregrounding the app will force it to re-request
  // a Reactivation notification.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [ChromeEarlGreyUI waitForAppToIdle];

  GREYAssert(notificationRequested,
             @"Reactivation notification request was not added.");
}

// Tests that the CPE Promo appears when tapping on the CPE tip notification.
- (void)testCPENotification {
  MaybeDismissNotification();
  [ChromeEarlGreyUI waitForAppToIdle];
  [self optInToTipsNotifications:{}];

  // Request the notification and tap it.
  [ChromeEarlGrey requestTipsNotification:TipsNotificationType::kCPE];
  TapNotification();
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(@"kCredentialProviderPromoAccessibilityId")];
  // Close the promo.
  id<GREYMatcher> noThanksButton =
      chrome_test_util::ButtonStackSecondaryButton();
  [ChromeEarlGrey waitForAndTapButton:noThanksButton];
}

@end
