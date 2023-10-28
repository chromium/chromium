// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/sync/base/features.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tabs/tests/distant_tabs_app_interface.h"
#import "ios/chrome/browser/ui/tabs/tests/fake_distant_tab.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

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

// Checks that the visibility of the tab resumption tile matches `should_show`.
void WaitUntilTabResumptionTileVisibleOrTimeout(bool should_show) {
  GREYCondition* tile_shown = [GREYCondition
      conditionWithName:@"Tile shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:
                                   grey_accessibilityID(l10n_util::GetNSString(
                                       IDS_IOS_TAB_RESUMPTION_TITLE))]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for the tile to be shown or timeout after kWaitForUIElementTimeout.
  BOOL success = [tile_shown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  if (should_show) {
    GREYAssertTrue(success, @"Tile did not appear.");
  } else {
    GREYAssertFalse(success, @"Tile appeared.");
  }
}

// Returns a GREYMatcher for the given label.
id<GREYMatcher> TabResumptionLabelMatcher(NSString* label) {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(kTabResumptionViewIdentifier)),
      grey_text(label), nil);
}

// Returns the hostname from the given `URL`.
NSString* HostnameFromGURL(GURL URL) {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

}  // namespace

// Test case for the tab reusmption tile.
@interface TabResumptionTestCase : ChromeTestCase

@end

@implementation TabResumptionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabResumption.name) + ":" +
      kTabResumptionParameterName + "/" + kTabResumptionAllTabsParam + "," +
      kMagicStack.name + "," + syncer::kSyncSessionOnVisibilityChanged.name);
  return config;
}

// Relaunches the app with start surface enabled.
- (void)relaunchAppWithStartSurfaceEnabled {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kStartSurface.name) + "<" +
      std::string(kStartSurface.name) + "," + kMagicStack.name + "," +
      kTabResumption.name);
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kStartSurface.name) + "/Test");
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kStartSurface.name) +
      ".Test:" + std::string(kReturnToStartSurfaceInactiveDurationInSeconds) +
      "/" + "0");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  SignInAndSync();
}

- (void)tearDown {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearSyncServerData];
  [ChromeEarlGrey resetDataForLocalStatePref:tab_resumption_prefs::
                                                 kTabResumptioDisabledPref];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref];
  [super tearDown];
}

// Tests that the tab resumption tile is correctly displayed for a distant tab.
- (void)testTabResumptionTileDisplayedForDistantTab {
  // Check that the tile is not displayed when there is no distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tile is displayed when there is a distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);
  [[EarlGrey
      selectElementWithMatcher:TabResumptionLabelMatcher(@"FROM \"DESKTOP\"")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:TabResumptionLabelMatcher(@"Tab 3")]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* footerLabel =
      [NSString stringWithFormat:@"%@ • %@",
                                 HostnameFromGURL(self.testServer->base_url()),
                                 @"5 mins ago"];
  [[EarlGrey selectElementWithMatcher:TabResumptionLabelMatcher(footerLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the tab resumption item.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabResumptionViewIdentifier)]
      performAction:grey_tap()];

  // Verify that the location bar shows the distant tab URL in a short form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            self.testServer->base_url().host())];
}

// Tests that the tab resumption tile is correctly displayed for a local tab.
- (void)testTabResumptionTileDisplayedForLocalTab {

  // Check that the tile is not displayed when there is no local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  // Relaunch the app.
  [self relaunchAppWithStartSurfaceEnabled];

  // Check that the tile is displayed when there is a local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);
  [[EarlGrey selectElementWithMatcher:TabResumptionLabelMatcher(@"ponies")]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* footerLabel =
      [NSString stringWithFormat:@"%@ • %@",
                                 HostnameFromGURL(self.testServer->base_url()),
                                 @"just now"];
  [[EarlGrey selectElementWithMatcher:TabResumptionLabelMatcher(footerLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the tab resumption item.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabResumptionViewIdentifier)]
      performAction:grey_tap()];

  // Verify that the location bar shows the local tab URL in a short form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            destinationUrl.host())];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
}

// Tests that interacting with the Shortcuts tile works when the tab resumption
// tile is displayed.
- (void)testInteractWithAnotherTile {
  // Check that the tile is not displayed when there is no distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tile is displayed when there is a distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);

  if (![ChromeEarlGrey isIPadIdiom]) {
    // Rotate iphone device so Magic Stack can be scrollable.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];
  }
  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                 IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 350)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  if (![ChromeEarlGrey isIPadIdiom]) {
    // Rotate iphone device back to portrait mode.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  }
}

// Tests that the context menu has the correct action and correctly hides the
// tile.
- (void)testTabResumptionTileLongPress {
  // Check that the tile is not displayed when there is no distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:@"Desktop"
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:4]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  // Check that the tile is displayed when there is a distant tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);

  // Long press the distant tab.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabResumptionViewIdentifier)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION))]
      performAction:grey_tap()];

  // Check that the tile is hidden.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);
}

@end
