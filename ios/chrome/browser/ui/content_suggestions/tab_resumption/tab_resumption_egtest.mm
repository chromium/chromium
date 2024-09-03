// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/sync/base/features.h"
#import "components/url_formatter/elide_url.h"
#import "components/visited_url_ranking/public/features.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/tabs/ui_bundled/tests/distant_tabs_app_interface.h"
#import "ios/chrome/browser/tabs/ui_bundled/tests/fake_distant_tab.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Sign in and enable history/tab sync using a fake identity.
void SignInAndEnableHistorySync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableHistorySync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Checks that the visibility of the tab resumption tile matches `should_show`.
void WaitUntilTabResumptionTileVisibleOrTimeout(bool should_show) {
  id<GREYMatcher> matcher =
      should_show ? grey_sufficientlyVisible() : grey_notVisible();
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(
                grey_accessibilityID(
                    kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier),
                grey_sufficientlyVisible(), nil)] assertWithMatcher:matcher
                                                              error:&error];
    return error == nil;
  };

  NSString* failure_reason = @"Tile visible.";
  if (should_show) {
    failure_reason = @"Tile did not appear.";
  }
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(6), condition),
      failure_reason);
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

- (BOOL)isUsingTabResumption15 {
  return [self.name containsString:@"TR15"];
}

- (BOOL)isUsingTabResumption2 {
  return [self.name containsString:@"TR2"];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isUsingTabResumption15]) {
    config.features_enabled.push_back(kTabResumption1_5);
  } else {
    config.features_disabled.push_back(kTabResumption1_5);
  }
  if ([self isUsingTabResumption2]) {
    config.features_enabled.push_back(kTabResumption2);
  } else {
    config.features_disabled.push_back(kTabResumption2);
  }
  config.additional_args.push_back(std::string("--") +
                                   kTabResumptionShowItemImmediately);
  config.additional_args.push_back("--test-ios-module-ranker=tab_resumption");
  // kVisitedURLRankingHistoryVisibilityScoreFilter require the network, keep
  // it disabled for tests.
  config.features_disabled.push_back(
      visited_url_ranking::features::
          kVisitedURLRankingHistoryVisibilityScoreFilter);
  return config;
}

// Relaunches the app with start surface enabled.
- (void)relaunchAppWithStartSurfaceEnabled {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kStartSurface.name) + "<" +
      std::string(kStartSurface.name));
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
  SignInAndEnableHistorySync();
  [NewTabPageAppInterface disableSetUpList];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

- (void)tearDown {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearFakeSyncServerData];
  [ChromeEarlGrey resetDataForLocalStatePref:tab_resumption_prefs::
                                                 kTabResumptioDisabledPref];
  [ChromeEarlGrey clearUserPrefWithName:tab_resumption_prefs::
                                            kTabResumptionLastOpenedTabURLPref];
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
  // Tab resumption 2 displays Tab0 in that case.
  NSString* displayedTab = [self isUsingTabResumption2] ? @"Tab 0" : @"Tab 3";
  [[EarlGrey selectElementWithMatcher:TabResumptionLabelMatcher(displayedTab)]
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

// Tests that the tab resumption 2 tile is correctly displayed for a distant
// tab.
// TODO(crbug.com/346713831): This test timed out on some configs.
- (void)DISABLED_testTabResumptionTileDisplayedForDistantTabTR2 {
  [self testTabResumptionTileDisplayedForDistantTab];
}

// Tests that the tab resumption tile is correctly displayed for a local tab.
- (void)testTabResumptionTileDisplayedForLocalTab {
  // TODO(crbug.com/333500324): Test failing on iPad device and simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test is flaky on iPad.")
  }

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

// Tests that the tab resumption 2 tile is correctly displayed for a local tab.
- (void)testTabResumptionTileDisplayedForLocalTabTR2 {
  [self testTabResumptionTileDisplayedForLocalTab];
}

// Tests that interacting with the Magic Stack edit button works when the tab
// resumption tile is displayed.
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

  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(grey_accessibilityID(
                                 kMagicStackEditButtonAccessibilityIdentifier),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeFastInDirection(kGREYDirectionLeft)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the tab resumption 2 tile is correctly displayed for a distant
// tab.
- (void)testInteractWithAnotherTileTR2 {
  [self testInteractWithAnotherTile];
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
                     IDS_IOS_MAGIC_STACK_CONTEXT_MENU_CUSTOMIZE_CARDS_TITLE))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION))]
      performAction:grey_tap()];

  // Check that the tile is hidden.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);
}

// Tests that the context menu has the correct action and correctly hides the
// tile.
// TODO(crbug.com/333500324): Test is flaky.
- (void)FLAKY_testTabResumptionTileLongPressTR2 {
  [self testTabResumptionTileLongPress];
}

- (void)testShowMoreVisibleTR15 {
  // Check that the tile is not displayed when there is no local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  // Relaunch the app.
  [self relaunchAppWithStartSurfaceEnabled];

  // Check that the tile is displayed when there is a local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"See more"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShowMoreNotVisible {
  // Check that the tile is not displayed when there is no local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(false);

  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  // Relaunch the app.
  [self relaunchAppWithStartSurfaceEnabled];

  // Check that the tile is displayed when there is a local tab.
  WaitUntilTabResumptionTileVisibleOrTimeout(true);
  NSError* error = nil;
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"See more"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  GREYAssertTrue(error, @"See more button is visible.");
}

@end
