// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/tabs/tests/distant_tabs_app_interface.h"
#import "ios/chrome/browser/ui/tabs/tests/fake_distant_tab.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Sign in and sync using a fake identity.
void SignInAndSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Matcher for the banner title.
id<GREYMatcher> BannerButtonMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_TAB_PICKUP_BANNER_BUTTON));
}

// Matcher for the banner open button.
id<GREYMatcher> BannerTitleMatcher(NSString* session_name) {
  NSString* titleText = l10n_util::GetNSStringF(
      IDS_IOS_TAB_PICKUP_BANNER_TITLE, base::SysNSStringToUTF16(session_name));
  return grey_accessibilityLabel(titleText);
}

// Checks that the visibility of the infobar matches `should_show`.
void WaitUntilInfobarBannerVisibleOrTimeout(bool should_show) {
  GREYCondition* infobar_shown = [GREYCondition
      conditionWithName:@"Infobar shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_accessibilityID(kInfobarBannerViewIdentifier)]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for infobar to be shown or timeout after kWaitForUIElementTimeout.
  BOOL success = [infobar_shown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  if (should_show) {
    GREYAssertTrue(success, @"Infobar does not appear.");
  } else {
    GREYAssertFalse(success, @"Infobar appeared.");
  }
}

}  // namespace

@interface TabPickupTestCase : ChromeTestCase

@end

@implementation TabPickupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabPickupThreshold);
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  SignInAndSync();
}

- (void)tearDown {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearSyncServerData];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [super tearDown];
}

// Verifies that the TabPickup banner is correctly displayed if the last tab
// was synced before the defined threshold.
- (void)testBannerVisibleBeforeThreshold {
  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is correctly displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:BannerTitleMatcher(@"Desktop")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the TabPickup banner is not displayed if the last tab was
// synced after the defined threshold.
- (void)testBannerNotVisibleAfterThreshold {
  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Hours(3)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is not displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(false);
}

// Verifies that tapping on the open button of the TabPickup banner correctly
// opens the distant tab.
- (void)testAcceptBanner {
  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is correctly displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:BannerTitleMatcher(@"Desktop")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  WaitUntilInfobarBannerVisibleOrTimeout(false);

  // Verify that the location bar shows the distant tab URL in a short form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            self.testServer->base_url().host())];
}

// Verifies that the TabPickup banner is displayed only once.
- (void)testBannerDisplayedOnce {
  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is correctly displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:BannerTitleMatcher(@"Desktop")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  WaitUntilInfobarBannerVisibleOrTimeout(false);

  // Create a new distant session with 1 tab.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop-2"
               modifiedTimeDelta:base::Minutes(2)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is not displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(false);
}

// Verifies that the TabPickup banner is displayed after backgrounding and
// foregrounding the app.
- (void)testBannerDisplayedAfterBackground {
  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop-1"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is correctly displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:BannerTitleMatcher(@"Desktop-1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  WaitUntilInfobarBannerVisibleOrTimeout(false);

  // Background and foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Create a new distant session with 1 tab.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop-2"
               modifiedTimeDelta:base::Minutes(2)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:1]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tabPickup banner is correctly displayed.
  WaitUntilInfobarBannerVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:BannerTitleMatcher(@"Desktop-2")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
