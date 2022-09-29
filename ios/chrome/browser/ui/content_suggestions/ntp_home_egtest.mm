// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/feed/core/v2/public/ios/pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}

// Returns a matcher, which is true if the view has its width equals to `width`.
id<GREYMatcher> OmniboxWidth(CGFloat width) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return fabs(view.bounds.size.width - width) < 0.001;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"Omnibox has correct width: %g",
                                              width]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns a matcher, which is true if the view has its width equals to `width`
// plus or minus `margin`.
id<GREYMatcher> OmniboxWidthBetween(CGFloat width, CGFloat margin) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return view.bounds.size.width >= width - margin &&
           view.bounds.size.width <= width + margin;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString
                       stringWithFormat:
                           @"Omnibox has correct width: %g with margin: %g",
                           width, margin]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}
}

// Test case for the NTP home UI. More precisely, this tests the positions of
// the elements after interacting with the device.
@interface NTPHomeTestCase : ChromeTestCase

@property(nonatomic, strong) NSString* defaultSearchEngine;

@end

@implementation NTPHomeTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [NTPHomeTestCase setUpHelper];
}

+ (void)setUpHelper {
  // Clear the pasteboard in case there is a URL copied, triggering an omnibox
  // suggestion.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  [self closeAllTabs];
}

+ (void)tearDown {
  [self closeAllTabs];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to enable the Discover feed for this test case.
  // Disabled elsewhere to account for possible flakiness.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableDiscoverFeed);
  config.features_enabled.push_back(kDiscoverFeedInNtp);
  return config;
}

- (void)setUp {
  [super setUp];
  [NewTabPageAppInterface setUpService];
  [NewTabPageAppInterface makeSuggestionsAvailable];
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(prefs::kArticlesForYouEnabled)];
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(feed::prefs::kArticlesListVisible)];

  self.defaultSearchEngine = [NewTabPageAppInterface defaultSearchEngine];
}

- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                error:nil];
  [NewTabPageAppInterface resetSearchEngineTo:self.defaultSearchEngine];

  [super tearDown];
}

#pragma mark - Tests

// Tests that all items are accessible on the home page.
// This is currently needed to prevent this test case from being ignored.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testAccessibility DISABLED_testAccessibility
#else
#define MAYBE_testAccessibility testAccessibility
#endif
- (void)MAYBE_testAccessibility {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the collections shortcut are displayed and working.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testCollectionShortcuts DISABLED_testCollectionShortcuts
#else
#define MAYBE_testCollectionShortcuts testCollectionShortcuts
#endif
- (void)MAYBE_testCollectionShortcuts {
  // Relaunch the app with trending queries disabled, to ensure that the
  // shortcuts module is always present.
  // TODO(crbug.com/1350826): Trending queries is configured as a
  // first-run trial, and one of the arms removes the Shortcuts
  // module. Fix these tests to force an appropriate configuration or
  // otherwise support the various possible experiment arms.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kTrendingQueriesModule);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Check the Bookmarks.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the ReadingList.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_READING_LIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the RecentTabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the History.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_HISTORY)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_HISTORY_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that when loading an invalid URL, the NTP is still displayed.
// Prevents regressions from https://crbug.com/1063154 .
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testInvalidURL DISABLED_testInvalidURL
#else
#define MAYBE_testInvalidURL testInvalidURL
#endif
- (void)MAYBE_testInvalidURL {
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad, because key '-' could not be "
                            @"found on the keyboard.");
  }
#endif  // !TARGET_IPHONE_SIMULATOR

  // TODO(crbug.com/1315304): Reenable.
  if ([ChromeEarlGrey isNewOmniboxPopupEnabled]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for new popup");
  }

  NSString* URL = @"app-settings://test/";

  // The URL needs to be typed to trigger the bug.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(URL)];

  // The first suggestion is a search, the second suggestion is the URL.
  id<GREYMatcher> rowMatcher =
      [ChromeEarlGrey isNewOmniboxPopupEnabled]
          ? grey_allOf(
                grey_ancestor(grey_accessibilityID(@"omnibox suggestion 0 1")),
                chrome_test_util::OmniboxPopupRow(),
                grey_accessibilityValue(URL), grey_sufficientlyVisible(), nil)
          : grey_allOf(
                grey_accessibilityID(@"omnibox suggestion 0 1"),
                chrome_test_util::OmniboxPopupRow(),
                grey_descendant(
                    chrome_test_util::StaticTextWithAccessibilityLabel(URL)),
                grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:rowMatcher] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the fake omnibox width is correctly updated after a rotation.
- (void)testOmniboxWidthRotation {

  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  UIEdgeInsets safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidth
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];

  collectionView = [NewTabPageAppInterface collectionView];
  safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the settings screen is shown.
- (void)testOmniboxWidthRotationBehindSettings {

  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  UIEdgeInsets safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidth
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [ChromeEarlGreyUI openSettingsMenu];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  collectionView = [NewTabPageAppInterface collectionView];
  safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the fake omnibox is pinned to the top.
- (void)testOmniboxPinnedWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];
  CGFloat collectionWidth =
      [NewTabPageAppInterface collectionView].bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");

  // The fake omnibox might be slightly bigger than the screen in order to cover
  // it for all screen scale.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidthBetween(collectionWidth + 1, 2)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];
  CGFloat collectionWidthAfterRotation =
      [NewTabPageAppInterface collectionView].bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
}

// Tests that the fake omnibox remains visible when scrolling, by pinning itself
// to the top of the NTP. Also ensures that NTP minimum height is respected.
- (void)testOmniboxPinsToTop {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad since it does not pin the omnibox.");
  }

  UIView* fakeOmnibox = [NewTabPageAppInterface fakeOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(fakeOmnibox.frame.origin.x > 1,
                 @"The omnibox is pinned to top before scrolling down.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];

  // After scrolling down, the omnibox should be pinned and visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(fakeOmnibox.frame.origin.x < 1,
                 @"The omnibox is not pinned to top when scrolling down, or "
                 @"the NTP cannot scroll.");
}

// Tests that the fake omnibox animation works, increasing the width of the
// omnibox.
- (void)testOmniboxWidthChangesWithScroll {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad since the width does not change for it.");
  }

  CGFloat omniboxWidthBeforeScrolling =
      [NewTabPageAppInterface fakeOmnibox].frame.size.width;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];

  CGFloat omniboxWidthAfterScrolling =
      [NewTabPageAppInterface fakeOmnibox].frame.size.width;

  GREYAssertTrue(
      omniboxWidthAfterScrolling > omniboxWidthBeforeScrolling,
      @"Fake omnibox width did not animate properly when scrolling.");
}

// Tests that the app doesn't crash when opening multiple tabs.
- (void)testOpenMultipleTabs {
  NSInteger numberOfTabs = 10;
  for (NSInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGreyUI openNewTab];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue([NSString
                            stringWithFormat:@"%@", @(numberOfTabs + 1)])];
}

// Tests that rotating to landscape and scrolling into the feed, opening another
// NTP, and then swtiching back retains the scroll position.
- (void)testOpenMultipleTabsandChangeOrientation {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [self testNTPInitialPositionAndContent:collectionView];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  CGFloat yOffsetBeforeSwitch = collectionView.contentOffset.y;

  [ChromeEarlGreyUI openNewTab];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  GREYAssertTrue(yOffsetBeforeSwitch ==
                     [NewTabPageAppInterface collectionView].contentOffset.y,
                 @"NTP scroll position not saved properly.");
}

// Tests that the promo is correctly displayed and removed once tapped.
- (void)testPromoTap {
  [NewTabPageAppInterface setWhatsNewPromoToMoveToDock];

  // Open a new tab to have the promo.
  // Need to close all NTPs to ensure NotificationPromo is reset when
  // kSingleNtp is enabled.
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewTab];

  // Tap the promo.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsWhatsNewIdentifier)]
      performAction:grey_tap()];

  // Promo dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsWhatsNewIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [NewTabPageAppInterface resetWhatsNewPromo];
}

// Tests that the position of the collection view is restored when navigating
// back to the NTP.
// TODO(crbug.com/1299362): Re-enable test after fixing flakiness.
- (void)DISABLED_testPositionRestored {
  [self addMostVisitedTile];

  // Add suggestions to be able to scroll on iPad.
  [NewTabPageAppInterface addNumberOfSuggestions:15
                        additionalSuggestionsURL:nil];

  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Navigate and come back.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position.");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// and moved up, the scroll position restored is the position before the omnibox
// is selected.
// Disable the test due to waterfall failure.
// TODO(crbug.com/1243222): enable the test with fix.
- (void)DISABLED_testPositionRestoredWithOmniboxFocused {
  [self addMostVisitedTile];

  // Add suggestions to be able to scroll on iPad.
  [NewTabPageAppInterface addNumberOfSuggestions:15
                        additionalSuggestionsURL:nil];

  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 150)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Navigate and come back.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertEqual(
      previousPosition, collectionView.contentOffset.y,
      @"NTP is not at the same position as before tapping the omnibox");
}

// Tests that tapping the fake omnibox focuses the real omnibox.
- (void)testTapFakeOmnibox {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  // TODO(crbug.com/1315304): Reenable.
  if ([ChromeEarlGrey isNewOmniboxPopupEnabled]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for new popup");
  }

  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  NSString* URL = base::SysUTF8ToNSString(pageURL.spec());
  // Tap the fake omnibox, type the URL in the real omnibox and navigate to the
  // page.
  [self focusFakebox];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([URL stringByAppendingString:@"\n"])];

  // Check that the page is loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
}

// Tests that tapping the fake omnibox moves the collection.
// TODO(crbug.com/1326523): enable once fixed on ios15-sdk-sim
- (void)DISABLED_testTapFakeOmniboxScroll {
  // Disable the test on iOS 15.3 due to build failure.
  // TODO(crbug.com/1306515): enable the test with fix.
  if (@available(iOS 15.3, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 15.3.");
  }
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Offset after the fake omnibox has been tapped.
  CGPoint offsetAfterTap = collectionView.contentOffset;

  // Make sure the fake omnibox has been hidden and the collection has moved.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  GREYAssertTrue(offsetAfterTap.y >= origin.y,
                 @"The collection has not moved.");

  // Unfocus the omnibox.
  [self unfocusFakeBox];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertEqual(
      origin.y, collectionView.contentOffset.y,
      @"The collection is not scrolled back to its previous position");
}

// Tests that tapping the fake omnibox then unfocusing it moves the collection
// back to where it was.
// TODO(crbug.com/1326523): enable once fixed on ios15-sdk-sim
- (void)DISABLED_testTapFakeOmniboxScrollScrolled {
  // Disable the test on iOS 15.3 due to build failure.
  // TODO(crbug.com/1306515): enable the test with fix.
  if (@available(iOS 15.3, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 15.3.");
  }
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Scroll to have a position different from the default.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Unfocus the omnibox.
  [self unfocusFakeBox];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The collection might be slightly moved on iPhone.
  GREYAssertTrue(
      collectionView.contentOffset.y >= origin.y &&
          collectionView.contentOffset.y <= origin.y + 6,
      @"The collection is not scrolled back to its previous position");
}

- (void)testOpeningNewTab {
  [ChromeEarlGreyUI openNewTab];

  // Check that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 2])];

  // Test the same thing after opening a tab from the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 3])];
}

// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testFavicons DISABLED_testFavicons
#else
#define MAYBE_testFavicons testFavicons
#endif
- (void)MAYBE_testFavicons {
  // Relaunch the app with trending queries disabled, to ensure that the
  // shortcuts module is always present.
  // TODO(crbug.com/1350826): Trending queries is configured as a
  // first-run trial, and one of the arms removes the Shortcuts
  // module. Fix these tests to force an appropriate configuration or
  // otherwise support the various possible experiment arms.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kTrendingQueriesModule);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Yahoo!.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Yahoo!")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Check again the favicons.
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Google.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Google")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// TODO(crbug.com/1255548): Add tests for overscroll menu.
// TODO(crbug.com/1353899): Test flaky.
- (void)DISABLED_testMinimumHeight {
  [ChromeEarlGreyAppInterface
      setBoolValue:NO
       forUserPref:base::SysUTF8ToNSString(prefs::kArticlesForYouEnabled)];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Ensures that tiles are still all visible with feed turned off after
  // scrolling.
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  // Ensures that fake omnibox visibility is correct.
  // On iPads, fake omnibox disappears and becomes real omnibox. On other
  // devices, fake omnibox persists and sticks to top.
  if ([ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        assertWithMatcher:grey_notVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Ensures that logo/doodle is no longer visible when scrolled down.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Test to ensure that initial position and content are maintained when rotating
// the device back and forth.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testInitialPositionAndOrientationChange \
  DISABLED_testInitialPositionAndOrientationChange
#else
#define MAYBE_testInitialPositionAndOrientationChange \
  testInitialPositionAndOrientationChange
#endif
- (void)MAYBE_testInitialPositionAndOrientationChange {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];

  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];

  [self testNTPInitialPositionAndContent:collectionView];
}

// Test to ensure that feed can be collapsed/shown and that feed header changes
// accordingly.
// TODO(crbug.com/1311947): Failing simulator.
- (void)DISABLED_testToggleFeedVisible {
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Hide feed.
  [self hideFeedFromNTPMenu];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:NO];
  [self checkIfNTPIsScrollable];

  // Show feed again.
  [self showFeedFromNTPMenu];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];
}

// Test to ensure that feed can be enabled/disabled and that feed header changes
// accordingly.
// TODO(crbug.com/1194106): Flaky on ios-simulator-noncq.
- (void)DISABLED_testToggleFeedEnabled {
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Disable feed.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(
                                kSettingsArticleSuggestionsCellId)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Ensure that label is no longer visible and that the NTP is still
  // scrollable.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_notVisible()];
  [self checkIfNTPIsScrollable];

  // Re-enable feed.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(
                                kSettingsArticleSuggestionsCellId)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Ensure that label is once again visible and that the NTP is still
  // scrollable.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];
}

// Test to ensure that NTP for incognito mode works properly.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testIncognitoMode DISABLED_testIncognitoMode
#else
#define MAYBE_testIncognitoMode testIncognitoMode
#endif
- (void)MAYBE_testIncognitoMode {
  // Checks that default NTP is not incognito.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Open tools menu and open incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolsMenuNewIncognitoTabId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Ensure that incognito view is visible and that the regular NTP is not.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPIncognitoView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notVisible()];

  // Reload page, then check if incognito view is still visible.
  if ([ChromeEarlGrey isNewOverflowMenuEnabled] &&
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
    // In the new
    // overflow menu on iPad, the reload button is only on the toolbar.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kToolsMenuReload)]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPIncognitoView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - New Tab menu tests

// Tests the "new search" menu item from the new tab menu.
// TODO(crbug.com/1280323): Re-enable after removing didLoadPageWithSuccess: to
// update NTP scroll state.
- (void)DISABLED_testNewSearchFromNewTabMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kToolsMenuSearch)]
      performAction:grey_tap()];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that there's now a new tab, that the new (second) tab is the active
  // one, and the that the omnibox is first responder.
  [ChromeEarlGrey waitForMainTabCount:2];

  GREYAssertEqual(1, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 1 should be active after starting a new search.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_notVisible()];
  // Fakebox should be covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests the "new search" menu item from the new tab menu after disabling the
// feed.
// TODO(crbug.com/1280323): Re-enable after removing didLoadPageWithSuccess: to
// update NTP scroll state.
- (void)DISABLED_testNewSearchFromNewTabMenuAfterTogglingFeed {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  // Hide feed.
  [self hideFeedFromNTPMenu];

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kToolsMenuSearch)]
      performAction:grey_tap()];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that there's now a new tab, that the new (third) tab is the active
  // one, and the that the omnibox is first responder.
  [ChromeEarlGrey waitForMainTabCount:2];

  GREYAssertEqual(1, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 1 should be active after starting a new search.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_notVisible()];

  // Fakebox should be covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests that the scroll position is maintained when switching from the Discover
// feed to the Following feed without fully scrolling into the feed.
// TODO(crbug.com/1330667): Re-enable when fixed.
- (void)DISABLED_testScrollPositionMaintainedWhenSwitchingFeedAboveFeed {
  if (![ChromeEarlGrey isWebChannelsEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Only applicable with Web Channels enabled.");
  }

  // Sign in to enable Following.
  [self signInThroughSettingsMenu];

  // Scrolls down a bit, not fully into the feed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Saves the content offset and switches to the Following feed.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat yOffsetBeforeSwitchingFeed = collectionView.contentOffset.y;

  [[EarlGrey selectElementWithMatcher:FeedHeaderSegmentFollowing()]
      performAction:grey_tap()];

  // Ensures that the new content offset is the same.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertEqual(yOffsetBeforeSwitchingFeed, collectionView.contentOffset.y,
                  @"Content offset is not the same after switching feeds.");
}

// Tests that the regular feed header is visible when signed out, and is swapped
// for the Following feed header after signing in.
// TODO(crbug.com/1330667): Re-enable when fixed.
- (void)DISABLED_testFollowingFeedHeaderIsVisibleWhenSignedIn {
  if (![ChromeEarlGrey isWebChannelsEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Only applicable with Web Channels enabled.");
  }

  // Check that regular feed header is visible when signed out, and not
  // Following header.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPFeedHeaderSegmentedControlIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign in to enable Following feed.
  [self signInThroughSettingsMenu];

  // Check that Following header is now visible, and not regular feed header.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPFeedHeaderSegmentedControlIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that feed ablation successfully hides the feed from the NTP and the
// toggle from the Chrome settings.
// TODO(crbug.com/1339419): Test fails on device.
// TODO(crbug.com/1350826): Test fails on small form factors.
- (void)DISABLED_testFeedAblationHidesFeed {
  // Relaunch the app with trending queries disabled, to ensure that the
  // discover feed is always present.
  // TODO(crbug.com/1350826): Trending queries is configured as a
  // first-run trial, and one of the arms removes the discover
  // feed. Fix these tests to force an appropriate configuration or
  // otherwise support the various possible experiment arms.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kTrendingQueriesModule);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Ensures that feed header is visible before enabling ablation.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Opens settings menu and ensures that Discover setting is present.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Relaunch the app with ablation enabled.
  config.features_enabled.push_back(kEnableFeedAblation);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Ensures that feed header is not visible with ablation enabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Opens settings menu and ensures that Discover setting is not present.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

- (void)addMostVisitedTile {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Clear history to ensure the tile will be shown.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

// Unfocus the omnibox.
- (void)unfocusFakeBox {
  if ([ChromeEarlGrey isIPadIdiom]) {
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:UIKeyInputEscape flags:0];
  } else {
    id<GREYMatcher> cancelButton =
        grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier);
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(cancelButton,
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  }
}

- (void)testNTPInitialPositionAndContent:(UICollectionView*)collectionView {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Check that feed label is visible with correct text for feed visibility.
- (void)checkFeedLabelForFeedVisible:(BOOL)visible {
  NSString* labelTextForVisibleFeed =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
  NSString* labelTextForHiddenFeed =
      [NSString stringWithFormat:@"%@ – %@", labelTextForVisibleFeed,
                                 l10n_util::GetNSString(
                                     IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  NSString* labelText =
      visible ? labelTextForVisibleFeed : labelTextForHiddenFeed;
  [EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::DiscoverHeaderLabel(),
                                   grey_sufficientlyVisible(), nil)];
  UILabel* discoverHeaderLabel = [NewTabPageAppInterface discoverHeaderLabel];
  GREYAssertTrue([discoverHeaderLabel.text isEqualToString:labelText],
                 @"Discover header label is incorrect");
}

// Check that NTP is scrollable by scrolling and comparing offsets, then return
// to top.
- (void)checkIfNTPIsScrollable {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat yOffsetBeforeScroll = collectionView.contentOffset.y;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  GREYAssertTrue(yOffsetBeforeScroll != collectionView.contentOffset.y,
                 @"NTP cannot be scrolled.");

  // Scroll back to top of NTP.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Usually a fast swipe scrolls back up, but in case it doesn't, make sure
  // by scrolling again, then slowly scrolling to the top.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
}

- (void)showFeedFromNTPMenu {
  bool feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertFalse(feed_visible, @"Expect feed to be hidden!");

  // The feed header button may be offscreen, so scroll to find it if needed.
  id<GREYMatcher> headerButton =
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderMenuButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NTPFeedMenuEnableButton()]
      performAction:grey_tap()];
  feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertTrue(feed_visible, @"Expect feed to be visible!");
}

- (void)hideFeedFromNTPMenu {
  bool feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertTrue(feed_visible, @"Expect feed to be visible!");

  // The feed header button may be offscreen, so scroll to find it if needed.
  id<GREYMatcher> headerButton =
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderMenuButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NTPFeedMenuDisableButton()]
      performAction:grey_tap()];
  feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertFalse(feed_visible, @"Expect feed to be hidden!");
}

// Opens the settings menu, signs in using a fake identity, and closes the menu.
- (void)signInThroughSettingsMenu {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PrimarySignInButton()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SettingsAccountButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Matchers

// Returns the segment of the feed header with a given title.
id<GREYMatcher> FeedHeaderSegmentedControlSegmentWithTitle(int title_id) {
  id<GREYMatcher> title_matcher =
      grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(title_id)),
                 grey_sufficientlyVisible(), nil);
  return grey_allOf(
      grey_accessibilityID(kNTPFeedHeaderSegmentedControlIdentifier),
      grey_descendant(title_matcher), grey_sufficientlyVisible(), nil);
}

// Returns the Discover segment of the feed header.
id<GREYMatcher> FeedHeaderSegmentDiscover() {
  return FeedHeaderSegmentedControlSegmentWithTitle(
      IDS_IOS_DISCOVER_FEED_TITLE);
}

// Returns the Following segment of the feed header.
id<GREYMatcher> FeedHeaderSegmentFollowing() {
  return FeedHeaderSegmentedControlSegmentWithTitle(
      IDS_IOS_FOLLOWING_FEED_TITLE);
}

@end

// Test case for the NTP home UI, except the new omnibox popup flag is enabled.
@interface NewOmniboxPopupNTPHomeTestCase : NTPHomeTestCase {
  // Which variant of the new popup flag to use.
  std::string _variant;
}
@end

@implementation NewOmniboxPopupNTPHomeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.additional_args.push_back(
      "--enable-features=" + std::string(kIOSOmniboxUpdatedPopupUI.name) + "<" +
      std::string(kIOSOmniboxUpdatedPopupUI.name));

  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kIOSOmniboxUpdatedPopupUI.name) +
      "/Test");

  config.additional_args.push_back(
      "--force-fieldtrial-params=" +
      std::string(kIOSOmniboxUpdatedPopupUI.name) + ".Test:" +
      std::string(kIOSOmniboxUpdatedPopupUIVariationName) + "/" + _variant);

  return config;
}

// Variants set the `--enable-features` flag manually, preventing us from being
// able to add more features without overriding the initial ones. We therefore
// skip this test for the variants since it relies on enabling a feature.
- (void)testFeedAblationHidesFeed {
  EARL_GREY_TEST_SKIPPED(@"Skipped for variants.");
}

@end

// Test case for the NTP home UI, except the new omnibox popup flag is enabled
// with variant 1.
@interface NewOmniboxPopupNTPHomeVariant1TestCase
    : NewOmniboxPopupNTPHomeTestCase
@end

@implementation NewOmniboxPopupNTPHomeVariant1TestCase

- (void)setUp {
  _variant = std::string(kIOSOmniboxUpdatedPopupUIVariation1);

  // `appConfigurationForTestCase` is called during [super setUp], and
  // depends on _variant.
  [super setUp];
}

// This is currently needed to prevent this test case from being ignored.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testEmpty DISABLED_testEmpty
#else
#define MAYBE_testEmpty testEmpty
#endif
- (void)MAYBE_testEmpty {
}

@end

// Test case for the NTP home UI, except the new omnibox popup flag is enabled
// with variant 2.
@interface NewOmniboxPopupNTPHomeVariant2TestCase
    : NewOmniboxPopupNTPHomeTestCase
@end

@implementation NewOmniboxPopupNTPHomeVariant2TestCase

- (void)setUp {
  _variant = std::string(kIOSOmniboxUpdatedPopupUIVariation2);

  // `appConfigurationForTestCase` is called during [super setUp], and
  // depends on _variant.
  [super setUp];
}

// This is currently needed to prevent this test case from being ignored.
// TODO(crbug.com/1339419): Test fails on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testEmpty DISABLED_testEmpty
#else
#define MAYBE_testEmpty testEmpty
#endif
- (void)MAYBE_testEmpty {
}

@end
