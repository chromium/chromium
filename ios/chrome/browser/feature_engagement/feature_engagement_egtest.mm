// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// The minimum number of times Chrome must be opened in order for the Reading
// List Badge to be shown.
const int kMinChromeOpensRequiredForReadingList = 5;

// The minimum number of times Chrome must be opened in order for the New Tab
// Tip to be shown.
const int kMinChromeOpensRequiredForNewTabTip = 3;

// Matcher for the Reading List Text Badge.
id<GREYMatcher> ReadingListTextBadge() {
  return grey_allOf(
      grey_accessibilityID(@"kToolsMenuTextBadgeAccessibilityIdentifier"),
      grey_ancestor(grey_allOf(grey_accessibilityID(kToolsMenuReadingListId),
                               grey_sufficientlyVisible(), nil)),
      nil);
}

// Matcher for the New Tab Tip Bubble.
id<GREYMatcher> NewTabTipBubble() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_TAB_IPH_PROMOTION_TEXT));
}

// Matcher for the Bottom Toolbar Tip Bubble.
id<GREYMatcher> BottomToolbarTipBubble() {
  return grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
      IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_TEXT));
}

// Matcher for the Long Press Tip Bubble.
id<GREYMatcher> LongPressTipBubble() {
  return grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
      IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_TEXT));
}

// Opens the TabGrid and then opens a new tab.
void OpenTabGridAndOpenTab() {
  id<GREYMatcher> openTabSwitcherMatcher =
      IsIPadIdiom() ? chrome_test_util::TabletTabSwitcherOpenButton()
                    : chrome_test_util::ShowTabsButton();
  [[EarlGrey selectElementWithMatcher:openTabSwitcherMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
}

// Opens and closes the tab switcher.
void OpenAndCloseTabSwitcher() {
  id<GREYMatcher> openTabSwitcherMatcher =
      IsIPadIdiom() ? chrome_test_util::TabletTabSwitcherOpenButton()
                    : chrome_test_util::ShowTabsButton();
  [[EarlGrey selectElementWithMatcher:openTabSwitcherMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Create a test FeatureEngagementTracker.
std::unique_ptr<KeyedService> CreateTestFeatureEngagementTracker(
    web::BrowserState* context) {
  return feature_engagement::CreateTestTracker();
}

// Simulate a Chrome Opened event for the Feature Engagement Tracker.
void SimulateChromeOpenedEvent() {
  feature_engagement::TrackerFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->NotifyEvent(feature_engagement::events::kChromeOpened);
}

// Loads the FeatureEngagementTracker.
void LoadFeatureEngagementTracker() {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      browserState, base::BindRepeating(&CreateTestFeatureEngagementTracker));
}

// Enables the Badged Reading List help to be triggered for |feature_list|.
void EnableBadgedReadingListTriggering(
    base::test::ScopedFeatureList& feature_list) {
  std::map<std::string, std::string> badged_reading_list_params;

  badged_reading_list_params["event_1"] =
      "name:chrome_opened;comparator:>=5;window:90;storage:90";
  badged_reading_list_params["event_trigger"] =
      "name:badged_reading_list_trigger;comparator:==0;window:1095;storage:"
      "1095";
  badged_reading_list_params["event_used"] =
      "name:viewed_reading_list;comparator:==0;window:90;storage:90";
  badged_reading_list_params["session_rate"] = "==0";
  badged_reading_list_params["availability"] = "any";

  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHBadgedReadingListFeature,
      badged_reading_list_params);
}

// Enables the New Tab Tip to be triggered for |feature_list|.
void EnableNewTabTipTriggering(base::test::ScopedFeatureList& feature_list) {
  std::map<std::string, std::string> new_tab_tip_params;

  new_tab_tip_params["event_1"] =
      "name:chrome_opened;comparator:>=3;window:90;storage:90";
  new_tab_tip_params["event_trigger"] =
      "name:new_tab_tip_trigger;comparator:<2;window:1095;storage:"
      "1095";
  new_tab_tip_params["event_used"] =
      "name:new_tab_opened;comparator:==0;window:90;storage:90";
  new_tab_tip_params["session_rate"] = "==0";
  new_tab_tip_params["availability"] = "any";

  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHNewTabTipFeature, new_tab_tip_params);
}

// Enables the Bottom Toolbar Tip to be triggered for |feature_list|.
void EnableBottomToolbarTipTriggering(
    base::test::ScopedFeatureList& feature_list) {
  std::map<std::string, std::string> bottom_toolbar_tip_params;

  bottom_toolbar_tip_params["availability"] = "any";
  bottom_toolbar_tip_params["session_rate"] = "==0";
  bottom_toolbar_tip_params["event_used"] =
      "name:bottom_toolbar_opened;comparator:any;window:90;storage:90";
  bottom_toolbar_tip_params["event_trigger"] =
      "name:bottom_toolbar_trigger;comparator:==0;window:90;storage:90";

  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHBottomToolbarTipFeature,
      bottom_toolbar_tip_params);
}

// Enables the Long Press Tip to be triggered for |feature_list|.
// The tip has a configuration where it can be displayed as first or second tip
// of the session and needs to be displayed after the BottomToolbar tip is
// displayed.
void EnableLongPressTipTriggering(base::test::ScopedFeatureList& feature_list) {
  std::map<std::string, std::string> long_press_tip_params;

  long_press_tip_params["availability"] = "any";
  long_press_tip_params["session_rate"] = "<=1";
  long_press_tip_params["event_used"] =
      "name:long_press_toolbar_opened;comparator:any;window:90;storage:90";
  long_press_tip_params["event_trigger"] =
      "name:long_press_toolbar_trigger;comparator:==0;window:90;storage:90";
  long_press_tip_params["event_1"] =
      "name:bottom_toolbar_opened;comparator:>=1;window:90;storage:90";

  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHLongPressToolbarTipFeature,
      long_press_tip_params);
}

}  // namespace

// Tests related to the triggering of In Product Help features.
@interface FeatureEngagementTestCase : ChromeTestCase
@end

@implementation FeatureEngagementTestCase

// Verifies that the Badged Reading List feature shows when triggering
// conditions are met. Also verifies that the Badged Reading List does not
// appear again after being shown.
- (void)testBadgedReadingListFeatureShouldShow {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableBadgedReadingListTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Ensure that Chrome has been launched enough times for the Badged Reading
  // List to appear.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    SimulateChromeOpenedEvent();
  }

  [ChromeEarlGreyUI openToolsMenu];

  [[[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Close tools menu by tapping reload.
  if (IsUIRefreshPhase1Enabled()) {
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     chrome_test_util::ReloadButton(),
                                     grey_ancestor(grey_accessibilityID(
                                         kPopupMenuToolsMenuTableViewId)),
                                     nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        performAction:grey_tap()];
  } else {
    [[[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        performAction:grey_tap()];
  }

  // Reopen tools menu to verify that the badge does not appear again.
  [ChromeEarlGreyUI openToolsMenu];
  // Make sure the ReadingList entry is visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuReadingListId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Reading List feature does not show if Chrome has
// not opened enough times.
- (void)testBadgedReadingListFeatureTooFewChromeOpens {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableBadgedReadingListTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Open Chrome just one time.
  SimulateChromeOpenedEvent();

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the Badged Reading List feature does not show if the reading
// list has already been used.
- (void)testBadgedReadingListFeatureReadingListAlreadyUsed {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableBadgedReadingListTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    SimulateChromeOpenedEvent();
  }

  [chrome_test_util::BrowserCommandDispatcherForMainBVC() showReadingList];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the New Tab Tip appears when all conditions are met.
- (void)testNewTabTipPromoShouldShow {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableNewTabTipTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForNewTabTip; index++) {
    SimulateChromeOpenedEvent();
  }

  // Navigate to a page other than the NTP to allow for the New Tab Tip to
  // appear.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Open and close the tab switcher to trigger the New Tab tip.
  OpenAndCloseTabSwitcher();

  // Verify that the New Tab Tip appeared.
  [[EarlGrey selectElementWithMatcher:NewTabTipBubble()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the New Tab Tip does not appear if all conditions are met,
// but the NTP is open.
- (void)testNewTabTipPromoDoesNotAppearOnNTP {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableNewTabTipTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForNewTabTip; index++) {
    SimulateChromeOpenedEvent();
  }

  // Open and close the tab switcher to potentially trigger the New Tab Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the New Tab Tip did not appear.
  [[EarlGrey selectElementWithMatcher:NewTabTipBubble()]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that the bottom toolbar tip is displayed when the phone is in split
// toolbar mode.
- (void)testBottomToolbarAppear {
  if (!IsUIRefreshPhase1Enabled())
    return;

  if (!IsSplitToolbarMode())
    return;

  base::test::ScopedFeatureList scoped_feature_list;

  EnableBottomToolbarTipTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Open and close the tab switcher to potentially trigger the Bottom Toolbar
  // Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom toolbar Tip appeared.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Bottom Toolbar tip to appear");
}

// Verifies that the bottom toolbar tip is not displayed when the phone is not
// in split toolbar mode.
- (void)testBottomToolbarDontAppearOnNonSplitToolbar {
  if (!IsUIRefreshPhase1Enabled())
    return;

  if (IsSplitToolbarMode())
    return;

  base::test::ScopedFeatureList scoped_feature_list;

  EnableBottomToolbarTipTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Open and close the tab switcher to potentially trigger the Bottom Toolbar
  // Tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom toolbar Tip appeared.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(!WaitUntilConditionOrTimeout(2, condition),
             @"The Bottom Toolbar tip shouldn't appear");
}

// Verifies that the LongPress tip is displayed only after the Bottom Toolbar
// tip is presented.
- (void)testLongPressTipAppearAfterBottomToolbar {
  if (!IsUIRefreshPhase1Enabled())
    return;

  if (!IsSplitToolbarMode())
    return;

  base::test::ScopedFeatureList scoped_feature_list_long_press;
  base::test::ScopedFeatureList scoped_feature_list_bottom_toolbar;

  EnableLongPressTipTriggering(scoped_feature_list_long_press);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list_long_press|.
  LoadFeatureEngagementTracker();

  // Open the tab switcher and open a new tab to try to trigger the tip.
  OpenTabGridAndOpenTab();

  // Verify that the Long Press Tip don't appear if the bottom toolbar tip
  // hasn't been displayed.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:LongPressTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      !WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
      @"The Long Press tip shouldn't appear before showing the other tip");

  // Enable the Bottom Toolbar tip.
  EnableBottomToolbarTipTriggering(scoped_feature_list_bottom_toolbar);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list_bottom_toolbar|.
  LoadFeatureEngagementTracker();

  // Open the tab switcher and open a new tab to try to trigger the tip.
  OpenAndCloseTabSwitcher();

  // Verify that the Bottom Toolbar tip has been displayed.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:BottomToolbarTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Bottom Toolbar tip.");

  // Open the tab switcher and open a new tab to try to trigger the LongPress
  // tip.
  OpenTabGridAndOpenTab();

  // Verify that the Long Press Tip appears now that the Bottom Toolbar tip has
  // been shown.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:LongPressTipBubble()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Waiting for the Long Press tip.");
}

@end
