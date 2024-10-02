// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/search_engines/model/search_engines_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

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

// Returns a matcher which is true if the view is not practically visible.
// Sometimes grey_notVisible() fails because the view is 0.0000XYZ percent
// visible, but not actually zero, probably due to some sort of floating
// point calculation.
id<GREYMatcher> notPracticallyVisible() {
  return grey_not(grey_minimumVisiblePercent(0.01));
}

// Returns a matcher which is true if the view is mostly not visible.
id<GREYMatcher> mostlyNotVisible() {
  return grey_not(grey_minimumVisiblePercent(0.33));
}

// Returns true if the difference between the two numbers is less than the
// margin of error
bool AreNumbersEqual(CGFloat num1, CGFloat num2) {
  int margin_of_error = 1;
  return abs(num1 - num2) < margin_of_error;
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
  // Disable search suggestions so that the omnibox popup does not appear.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSearchSuggestEnabled];

  [self closeAllTabs];
  [ChromeEarlGrey clearBrowsingHistory];
}

+ (void)tearDown {
  [self closeAllTabs];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to enable the Discover feed for this test case.
  // Disabled elsewhere to account for possible flakiness.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Make sure the search engine country is set, for `testFavicons` test.
  config.additional_args.push_back(
      std::string("--") + switches::kSearchEngineChoiceCountry + "=US");
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableDiscoverFeed);
  // Show doodle to make sure tests cover async callback logic updating logo.
  config.additional_args.push_back(
      std::string("-google-doodle-url=https://www.gstatic.com/chrome/ntp/"
                  "doodle_test/ddljson_android0.json"));
  config.features_disabled.push_back(kEnableFeedAblation);
  config.features_disabled.push_back(kSafetyCheckMagicStack);

  if ([self isRunningTest:@selector(testLargeFakeboxFocus)]) {
    config.features_enabled.push_back(kIOSLargeFakebox);
  }

  if ([self isRunningTest:@selector(testCollectionShortcuts)]) {
    // This ensures that the test will not fail when What's New is updated.
    config.additional_args.push_back(base::StringPrintf(
        "--disable-features=%s",
        feature_engagement::kIPHWhatsNewUpdatedFeature.name));
  }

  if ([self isRunningTest:@selector(testSignInSignOutScrolledToTop)]) {
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
  } else if ([self isRunningTest:@selector
                   (testSignInSignOutScrolledToTop_AccountMenu)]) {
    config.features_enabled.push_back(kIdentityDiscAccountMenu);
  }

  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kArticlesForYouEnabled];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:feed::prefs::kArticlesListVisible];

  self.defaultSearchEngine = [SearchEnginesAppInterface defaultSearchEngine];
  [NewTabPageAppInterface disableSetUpList];
}

- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [SearchEnginesAppInterface setSearchEngineTo:self.defaultSearchEngine];

  [self resetCustomizationPrefs];

  [super tearDown];
}

#pragma mark - Tests

// Tests that all items are accessible on the home page.
// This is currently needed to prevent this test case from being ignored.
- (void)testAccessibility {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the collections shortcut are displayed and working.
- (void)testCollectionShortcuts {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Close NTP and reopen.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

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

// Tests that the collections shortcut are displayed and working.
- (void)testCollectionShortcutsWithWhatsNew {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // This ensures that the test will not fail when What's New has already been
  // opened during testing.
  config.iph_feature_enabled =
      feature_engagement::kIPHWhatsNewUpdatedFeature.name;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Navigate
  // TODO(crbug.com/41483080): The FET is not ready upon app launch in the NTP.
  // Consequently, close NTP and reopen the NTP where the FET becomes ready.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Check the What's New.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_WHATS_NEW)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_WHATS_NEW)]
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
- (void)testInvalidURL {
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad, because key '-' could not be "
                            @"found on the keyboard.");
  }
#endif  // !TARGET_IPHONE_SIMULATOR

  NSString* URL = @"app-settings://test/";

  // The URL needs to be typed to trigger the bug.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(URL)];

  // The first suggestion is a search, the second suggestion is the URL.
  id<GREYMatcher> rowMatcher = grey_allOf(
      grey_accessibilityID(@"omnibox suggestion 0 1"),
      chrome_test_util::OmniboxPopupRow(),
      grey_descendant(chrome_test_util::StaticTextWithAccessibilityLabel(URL)),
      grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:rowMatcher] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}
// Tests that the Search Widget URL loads the NTP with the Omnibox focused.
- (void)testOpenSearchWidget {
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/search")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  // Fakebox should be mostly covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:mostlyNotVisible()];
}

// Tests that the fake omnibox width is correctly updated after a rotation.
- (void)testOmniboxWidthRotation {
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
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the settings screen is shown.
- (void)testOmniboxWidthRotationBehindSettings {
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
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the fake omnibox is pinned to the top.
- (void)testOmniboxPinnedWidthRotation {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fake Omnibox is not pinned to the top on iPad");
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];
  CGFloat NTPWidth = [NewTabPageAppInterface NTPView].bounds.size.width;
  GREYAssertTrue(NTPWidth > 0, @"The NTP width is nil.");

  // The fake omnibox might be slightly bigger than the screen in order to cover
  // it for all screen scale.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidthBetween(NTPWidth + 1, 2)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  UIEdgeInsets safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  CGFloat fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
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

// Tests that the tap gesture recognizer that dismisses the keyboard and
// defocuses the omnibox works.
- (void)testDefocusOmniboxTapWorks {
  [self focusFakebox];
  // Tap on a space in the collectionView that is not a Feed card.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsCollectionIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];
  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
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
// TODO(crbug.com/370968166): Test flaky on iphone-device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testOpenMultipleTabsandChangeOrientation \
  testOpenMultipleTabsandChangeOrientation
#else
#define MAYBE_testOpenMultipleTabsandChangeOrientation \
  DISABLED_testOpenMultipleTabsandChangeOrientation
#endif
- (void)MAYBE_testOpenMultipleTabsandChangeOrientation {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [self testNTPInitialPositionAndContent:collectionView];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  CGFloat yOffsetBeforeSwitch = collectionView.contentOffset.y;

  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  GREYAssertTrue(yOffsetBeforeSwitch ==
                     [NewTabPageAppInterface collectionView].contentOffset.y,
                 @"NTP scroll position not saved properly.");
}

// Tests that the pull to refresh (iphone) or the refresh button (ipad) lands
// the user on the top of the NTP even with a previously saved scroll position.
- (void)testReload {
  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position.");

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Have to scroll up to the top since tapping on reload button does not
    // automatically scroll to the top when feed is off or if feed returns no
    // contents (e.g. upstream bots). TODO(crbug.com/40252945): Look into why
    // the Feed only scrolls up when there is content.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
    // Tap on reload button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
        performAction:grey_tap()];
  } else {
    // Get back to the top of the page and then pull down to trigger Pull To
    // Refresh
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
}

// Tests that the position of the collection view is restored when navigating
// back to the NTP.
- (void)testPositionRestored {
  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position.");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// and moved up, the scroll position restored is the position before the omnibox
// is selected.
- (void)testPositionRestoredWithShiftingOffset {
  // Scroll a bit to have a position to restore.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 20)];

  // Save the position before focusing the omnibox.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same as before focusing the omnibox.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertTrue(
      AreNumbersEqual(previousPosition, collectionView.contentOffset.y),
      @"NTP is not at the same position as before tapping the omnibox");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// does not consider the shifting offset in the instance the omnibox was already
// pinned to the top of the page before focusing.
- (void)testPositionRestoredWithoutShiftingOffset {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Pinning Fake Omnibox to top of surface is only on iphone");
  }

  // Scroll enough to naturally pin the omnibox to the top.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Ensure that focusing the omnibox doesn't change the scroll position.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"Focusing the omnibox changed the scroll position.");

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  collectionView = [NewTabPageAppInterface collectionView];

  GREYAssertTrue(
      AreNumbersEqual(previousPosition, collectionView.contentOffset.y),
      @"NTP is not at the same position");
}

// Tests that tapping the fake omnibox focuses the real omnibox.
- (void)testTapFakeOmnibox {
  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  NSString* URL = base::SysUTF8ToNSString(pageURL.spec());
  // Tap the fake omnibox, type the URL in the real omnibox and navigate to the
  // page.
  [self focusFakebox];

  // Check the fake omnibox is mostly not visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:mostlyNotVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(URL)];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Check that the page is loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
}

// Tests that tapping the fake omnibox moves the collection.
- (void)testTapFakeOmniboxScroll {
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
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertTrue(
      AreNumbersEqual(origin.y, collectionView.contentOffset.y),
      @"The collection is not scrolled back to its previous position");
}

// Tests that tapping the fake omnibox and then scrolling defocuses the the
// omnibox.
- (void)testTapFakeOmniboxAndScrollDefocuses {
  // Clear pasteboard so that omnibox doesn't cover the NTP on focus.
  [ChromeEarlGrey clearPasteboard];
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Offset after the fake omnibox has been tapped.
  CGPoint offsetAfterTap = collectionView.contentOffset;

  // Make sure the fake omnibox has been mostly covered and the collection has
  // moved.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:mostlyNotVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(offsetAfterTap.y >= origin.y,
                 @"The collection has not moved.");

  // Scroll up.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // iPad needs more scrolling to see entire fake omnibox since it appears
    // from under the toolbar.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionUp, 100)];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionUp, 50)];
  }

  // Check the fake omnibox is displayed again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that tapping the fake omnibox then unfocusing it moves the collection
// back to where it was.
- (void)testTapFakeOmniboxScrollScrolled {
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
  [ChromeEarlGreyUI waitForAppToIdle];

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

- (void)testFavicons {
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
  NSString* yahooSearchEngineName = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::yahoo];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(yahooSearchEngineName)]
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
  // Scroll down if the shortcuts may not be completely in view due to Trending
  // Queries.
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID([NSString
                  stringWithFormat:
                      @"%@0",
                      kContentSuggestionsShortcutsAccessibilityIdentifierPrefix]),
              grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
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

- (void)testMinimumHeight {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kArticlesForYouEnabled];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  GREYWaitForAppToIdle(@"App failed to idle");

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

  // Just check for Magic Stack interactibility since the top module shown may
  // vary.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

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
      assertWithMatcher:notPracticallyVisible()];
}

// Test to ensure that initial position and content are maintained when rotating
// the device back and forth.
- (void)testInitialPositionAndOrientationChange {
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
- (void)testToggleFeedVisible {
  // Disable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
- (void)testToggleFeedEnabled {
  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Disable feed.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                           kSettingsArticleSuggestionsCellId,
                                           /*is_toggled_on=*/YES,
                                           /*enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 350)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
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
  // Why do we we not reset the scroll position after bringing back the feed as
  // the new parent collectionview? on iphone 8 the logo is cut off.
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                           kSettingsArticleSuggestionsCellId,
                                           /*is_toggled_on=*/NO,
                                           /*enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 350)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
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
- (void)testIncognitoMode {
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

// Tests that the Magic Stack feature swipeable when the
// kMagicStackMostVisitedModuleParam param is true. Most Visited Tiles and
// Shortcuts should be visible.
- (void)testMagicStack {
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  std::string enable_mvt_arg = std::string(kMagicStack.name) + ":" +
                               kMagicStackMostVisitedModuleParam + "/true";
  config.additional_args.push_back("--enable-features=" + enable_mvt_arg);
  config.additional_args.push_back("--test-ios-module-ranker=mvt");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  id<GREYMatcher> magicStackScrollView =
      grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier);

  // Scroll down to find the MagicStack.
  [[[EarlGrey selectElementWithMatcher:magicStackScrollView]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Verify Most Visited Tiles module title is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe to next module
  // Need to swipe at least half of the widest a module can be.
  CGFloat moduleSwipeAmount = 250;
  [[EarlGrey selectElementWithMatcher:magicStackScrollView]
      performAction:GREYScrollInDirectionWithStartPoint(
                        kGREYDirectionRight, moduleSwipeAmount, 0.9, 0.5)];

  // Verify Shortcuts module title is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_SHORTCUTS_MODULE_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the Bookmarks.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the ReadingList.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the RecentTabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the History.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_HISTORY)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe back to first module
  [[EarlGrey selectElementWithMatcher:magicStackScrollView]
      performAction:GREYScrollInDirectionWithStartPoint(
                        kGREYDirectionLeft, moduleSwipeAmount, 0.10, 0.5)];

  // Verify Most Visited Tiles module title is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that signing in and signing out results in the NTP scrolled to the top
// and not in some unexpected layout state.
- (void)testSignInSignOutScrolledToTop {
// TODO(crbug.com/40903244): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:identity];
  [SigninEarlGrey signinWithFakeIdentity:identity];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Verify Identity Disc is visible since it is the top-most element and should
  // be showing now.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSStringF(
                                   IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
                                   base::SysNSStringToUTF16(
                                       identity.userFullName),
                                   base::SysNSStringToUTF16(
                                       identity.userEmail)))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGreyUI signOut];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that signing in and signing out results in the NTP scrolled to the top
// and not in some unexpected layout state.
- (void)testSignInSignOutScrolledToTop_AccountMenu {
// TODO(crbug.com/40903244): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:identity];
  [SigninEarlGrey signinWithFakeIdentity:identity];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Verify Identity Disc is visible since it is the top-most element and should
  // be showing now.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSStringF(
              IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU,
              base::SysNSStringToUTF16(identity.userFullName),
              base::SysNSStringToUTF16(identity.userEmail)))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGreyUI signOut];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that the omnibox remains focused with some inputted text after
// backgrounding and foregrounding the app.
- (void)testRetainOmniboxFocusOnBackground {
  // Focus the omnibox and type some text into it.
  [self focusFakebox];
  NSString* omniboxText = @"Some text";
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(omniboxText)];

  // Check that the omnibox contains the inputted text.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(omniboxText))];

  // Background and foreground the app, then check that the focused omnibox
  // still contains the text.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(omniboxText))];
}

// Test that the Large Fakebox can be focused and text can be entered.
- (void)testLargeFakeboxFocus {
  // Focus the omnibox and type some text into it.
  [self focusFakebox];
  NSString* omniboxText = @"Some text";
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(omniboxText)];

  // Check that the omnibox contains the inputted text.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(omniboxText))];
}

#pragma mark - New Tab menu tests

// Tests the "new search" menu item from the new tab menu.
- (void)testNewSearchFromNewTabMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_NEW_SEARCH)] performAction:grey_tap()];
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
  // Fakebox should be mostly covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:mostlyNotVisible()];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests the "new search" menu item from the new tab menu after disabling the
// feed.
- (void)testNewSearchFromNewTabMenuAfterTogglingFeed {
  // Enable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  // Disable feed.
  [self disableFeedFromCustomizationMenu];

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_NEW_SEARCH)] performAction:grey_tap()];
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

  // Fakebox should be mostly covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests that the scroll position is maintained when switching from the Discover
// feed to the Following feed without fully scrolling into the feed.
// TODO(crbug.com/40239216): Re-enable when fixed.
- (void)DISABLED_testScrollPositionMaintainedWhenSwitchingFeedAboveFeed {
  if (![ChromeEarlGrey isWebChannelsEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Only applicable with Web Channels enabled.");
  }

  // Sign in to enable Following.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

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
// TODO(crbug.com/40239216): Re-enable when fixed.
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
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

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
// TODO(crbug.com/40856730): Test fails on small form factors.
- (void)DISABLED_testFeedAblationHidesFeed {
  // Relaunch the app with trending queries disabled, to ensure that the
  // discover feed is always present.
  // TODO(crbug.com/40856730): Trending queries is configured as a
  // first-run trial, and one of the arms removes the discover
  // feed. Fix these tests to force an appropriate configuration or
  // otherwise support the various possible experiment arms.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
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

// Tests that content suggestions are hidden for supervised users on sign-in.
// When the supervised user signs out the active policy should apply to the NTP.
- (void)testFeedHiddenForSupervisedUser {
  // Disable trending queries experiment to ensure that the Discover feed is
  // visible when first opening the NTP.
  // TODO(crbug.com/40856730): Adapt the test with launch of trending queries.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(std::string("--") +
                                   switches::kDisableSearchEngineChoiceScreen);
  config.features_enabled.push_back(
      supervised_user::
          kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];

  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Check that the feed label is not visible and if NTP is scrollable.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [self checkIfNTPIsScrollable];

  // Check that the fake omnibox is visible.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::FakeOmnibox()];

  // Opens settings menu and ensures that Discover setting is not present.
  [self checkDiscoverSettingsToggleVisible:NO];

  [SigninEarlGreyUI signOut];

  // The feed label should be visible on sign-out.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];
}

// Tests that content suggestions are hidden for supervised users on sign-in,
// with supervision status based on system capabilities.
// TODO(crbug.com/346756363): Remove this test when supervision status system
// capabilities are deprecated.
- (void)testFeedHiddenForSupervisedUserViaSystemCapabilities {
  // Disable trending queries experiment to ensure that the Discover feed is
  // visible when first opening the NTP.
  // TODO(crbug.com/40856730): Adapt the test with launch of trending queries.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(std::string("--") +
                                   switches::kDisableSearchEngineChoiceScreen);
  config.features_disabled.push_back(
      supervised_user::
          kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];

  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Check that the feed label is not visible and if NTP is scrollable.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [self checkIfNTPIsScrollable];

  // Check that the fake omnibox is visible.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::FakeOmnibox()];

  // Opens settings menu and ensures that Discover setting is not present.
  [self checkDiscoverSettingsToggleVisible:NO];

  [SigninEarlGreyUI signOut];

  // The feed label should be visible on sign-out.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];
}

// Tests that the feed top sync promo is visible when conditions are met, and
// that pressing the dismiss button makes it disappear.
// TODO(crbug.com/40252918): Enable test when feed is supported.
- (void)DISABLED_testFeedTopSyncPromoIsVisibleAndDismiss {
  // Scroll into feed to trigger engagement condition.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Relaunch the app
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Scroll down a bit and check that the promo is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 100)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSigninPromoViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the dismiss button and check that the promo is no longer visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSigninPromoCloseButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSigninPromoViewId)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

#pragma mark - Customization tests

// Tests that the customization menu can be used to toggle the visibility of
// Home surface modules.
- (void)testToggleModuleVisiblityInCustomizationMenu {
  // Enable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self resetCustomizationPrefs];

  // Open the Home customization menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  // Check for a toggle cell for Shortcuts, Magic Stack and Discover, and ensure
  // that they're all on.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMostVisitedIdentifier)]
      assertWithMatcher:grey_switchWithOnState(YES)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      assertWithMatcher:grey_switchWithOnState(YES)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleDiscoverIdentifier)]
      assertWithMatcher:grey_switchWithOnState(YES)];

  // Turn off the Magic Stack and Discover toggles.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      performAction:grey_turnSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleDiscoverIdentifier)]
      performAction:grey_turnSwitchOn(NO)];

  // Dismiss the customization menu and check that only the Shortcuts are
  // visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNavigationBarDismissButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsCollectionIdentifier)]
      assertWithMatcher:grey_not(grey_notVisible())];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Re-open the menu and check that the toggles retained the correct state.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID([HomeCustomizationHelper
                     navigationBarTitleForPage:CustomizationMenuPage::kMain])]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMostVisitedIdentifier)]
      assertWithMatcher:grey_switchWithOnState(YES)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      assertWithMatcher:grey_switchWithOnState(NO)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleDiscoverIdentifier)]
      assertWithMatcher:grey_switchWithOnState(NO)];

  // Toggle different modules and check that their visibility was properly
  // modified.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      performAction:grey_turnSwitchOn(YES)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleDiscoverIdentifier)]
      performAction:grey_turnSwitchOn(YES)];
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMostVisitedIdentifier)]
      performAction:grey_turnSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNavigationBarDismissButtonIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsCollectionIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_notVisible())];
  [self checkFeedLabelForFeedVisible:YES];
}

// Tests that the toggles in the main page of the customization menu can be used
// to navigate to their respective submenus.
- (void)testNavigateInCustomizationMenu {
  // Enable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self resetCustomizationPrefs];

  // Open the Home customization menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  // Tap the Most Visited cell which shouldn't prompt a navigation.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kCustomizationToggleMostVisitedNavigableIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID([HomeCustomizationHelper
                     navigationBarTitleForPage:CustomizationMenuPage::kMain])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Disable Magic Stack which should disable navigation.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      performAction:grey_turnSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kCustomizationToggleMagicStackNavigableIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID([HomeCustomizationHelper
                     navigationBarTitleForPage:CustomizationMenuPage::kMain])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Re-enable the Magic Stack switch and tap it to check for a navigation to
  // its submenu.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleMagicStackIdentifier)]
      performAction:grey_turnSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kCustomizationToggleMagicStackNavigableIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([HomeCustomizationHelper
              navigationBarTitleForPage:CustomizationMenuPage::kMagicStack])]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the Discover submenu of the Home customization menu.
- (void)testCustomizationDiscoverSubmenu {
  // Enable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self resetCustomizationPrefs];

  // Open the Home customization menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  // Navigate to the Discover submenu.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kCustomizationToggleDiscoverNavigableIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([HomeCustomizationHelper
              navigationBarTitleForPage:CustomizationMenuPage::kDiscover])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that all 4 link cells are visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kCustomizationLinkFollowingIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCustomizationLinkHiddenIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCustomizationLinkActivityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kCustomizationCollectionDiscoverIdentifier)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kCustomizationLinkLearnMoreIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap a cell and check that the menu is no longer visible, indicating that a
  // navigation occurred.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCustomizationLinkHiddenIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kCustomizationLinkHiddenIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

#pragma mark - Helpers

// Opens the Settings menu and ensures that the visibility of the Discover
// option matches the `visible` parameter.
- (void)checkDiscoverSettingsToggleVisible:(BOOL)visible {
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:visible ? grey_notNil() : grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

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
    // "escape" is a hardcoded key string in hardware_keyboard_util that maps to
    // a HIDUsageCode.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"escape" flags:0];
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_sufficientlyVisible()];
  UILabel* discoverHeaderLabel = [NewTabPageAppInterface discoverHeaderLabel];
  GREYAssertTrue([discoverHeaderLabel.text isEqualToString:labelText],
                 @"Discover header label is incorrect");
}

// Check that NTP is scrollable by scrolling and comparing offsets, then return
// to top.
- (void)checkIfNTPIsScrollable {
  // The custom tab strip on iPad causes an infinite animation that blocks
  // EarlGrey from continuing.
  // TODO(crbug.com/40237121): Remove iPad condition when scrolling is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }
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
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderManagementButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::NTPFeedMenuEnableButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
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
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderManagementButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NTPFeedMenuDisableButton()]
      performAction:grey_tap()];
  // This ensures that the app is given time to update the pref before checking
  // its state.
  [ChromeEarlGreyUI waitForAppToIdle];
  feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertFalse(feed_visible, @"Expect feed to be hidden!");
}

// Resets the preferences related to Home customization.
- (void)resetCustomizationPrefs {
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kHomeCustomizationMostVisitedEnabled];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kHomeCustomizationMagicStackEnabled];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kArticlesForYouEnabled];
}

// Disables the Discover feed from the Home Customization menu.
- (void)disableFeedFromCustomizationMenu {
  // Open the customization menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  // Disable the Discover feed.
  [[EarlGrey
      selectElementWithMatcher:CustomizationToggle(
                                   kCustomizationToggleDiscoverIdentifier)]
      performAction:grey_turnSwitchOn(NO)];

  // Close the customization menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNavigationBarDismissButtonIdentifier)]
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

// Returns the switch in toggle cell from the customization menu.
id<GREYMatcher> CustomizationToggle(NSString* identifier) {
  return grey_allOf(grey_kindOfClassName(@"UISwitch"),
                    grey_ancestor(grey_accessibilityID(identifier)), nil);
}

@end
