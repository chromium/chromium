// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Keys of UMA IOS.TabSwitcher.Idle histograms.
const char kUMATabSwitcherIdleIncognitoTabGridPageHistogram[] =
    "IOS.TabSwitcher.Idle.IncognitoTabGridPage";
const char kUMATabSwitcherIdleRecentTabsHistogram[] =
    "IOS.TabSwitcher.Idle.RecentTabs";
const char kUMATabSwitcherIdleRegularTabGridPageHistogram[] =
    "IOS.TabSwitcher.Idle.RegularTabGridPage";

// Accessibility identifiers for automated testing.
NSString* const kTabGridIncognitoTabsPageButtonIdentifier =
    @"TabGridIncognitoTabsPageButtonIdentifier";
NSString* const kTabGridRegularTabsPageButtonIdentifier =
    @"TabGridRegularTabsPageButtonIdentifier";
NSString* const kTabGridRemoteTabsPageButtonIdentifier =
    @"TabGridRemoteTabsPageButtonIdentifier";
NSString* const kTabGridDoneButtonIdentifier = @"TabGridDoneButtonIdentifier";
NSString* const kTabGridCancelButtonIdentifier =
    @"TabGridCancelButtonIdentifier";
NSString* const kTabGridSearchButtonIdentifier =
    @"TabGridSearchButtonIdentifier";
NSString* const kTabGridCloseAllButtonIdentifier =
    @"TabGridCloseAllButtonIdentifier";
NSString* const kTabGridUndoCloseAllButtonIdentifier =
    @"TabGridUndoCloseAllButtonIdentifier";
NSString* const kTabGridIncognitoTabsEmptyStateIdentifier =
    @"TabGridIncognitoTabsEmptyStateIdentifier";
NSString* const kTabGridRegularTabsEmptyStateIdentifier =
    @"TabGridRegularTabsEmptyStateIdentifier";
NSString* const kTabGridScrollViewIdentifier = @"kTabGridScrollViewIdentifier";
NSString* const kRegularTabGridIdentifier = @"kRegularTabGridIdentifier";
NSString* const kIncognitoTabGridIdentifier = @"kIncognitoTabGridIdentifier";
NSString* const kInactiveTabGridIdentifier = @"kInactiveTabGridIdentifier";

NSString* const kTabGridEditButtonIdentifier = @"kTabGridEditButtonIdentifier";
NSString* const kTabGridEditCloseTabsButtonIdentifier =
    @"kTabGridEditCloseTabsButtonIdentifier";
NSString* const kTabGridEditSelectAllButtonIdentifier =
    @"kTabGridEditSelectAllButtonIdentifier";
NSString* const kTabGridEditAddToButtonIdentifier =
    @"kTabGridEditAddToButtonIdentifier";
NSString* const kTabGridEditShareButtonIdentifier =
    @"kTabGridEditShareButtonIdentifier";
NSString* const kTabGridSearchBarIdentifier = @"kTabGridSearchBarIdentifier";
NSString* const kTabGridSearchTextFieldIdentifierPrefix = @"kSearchTextId_";
NSString* const kTabGridScrimIdentifier = @"kTabGridScrimIdentifier";

// The color of the text buttons in the toolbars.
const int kTabGridToolbarTextButtonColor = 0xFFFFFF;

// Colors for the empty state and disabled tab view.
const int kTabGridEmptyStateTitleTextColor = 0xF8F9FA;
const int kTabGridEmptyStateBodyTextColor = 0xBDC1C6;

// The distance the toolbar content is inset from either side.
const CGFloat kTabGridToolbarHorizontalInset = 16.0f;

// The distance between the title and body of the empty state view.
const CGFloat kTabGridEmptyStateVerticalMargin = 4.0f;

// The insets from the edges for empty state.
extern const CGFloat kTabGridEmptyStateVerticalInset = 17.0f;
extern const CGFloat kTabGridEmptyStateHorizontalInset = 80.0f;

// The insets from the edges for the floating button.
const CGFloat kTabGridFloatingButtonVerticalInset = 28.0f;
const CGFloat kTabGridFloatingButtonHorizontalInset = 20.0f;

// The Search bar original width ratio of the available space from the
// containing toolbar before any width modifiers.
const CGFloat kTabGridSearchBarWidthRatio = 0.9f;
// The tab grid Search bar height.
const CGFloat kTabGridSearchBarHeight = 44.0f;
// The Search bar width ratio modifier for non-compact orientation.
const CGFloat kTabGridSearchBarNonCompactWidthRatioModifier = 0.5f;

// Intrinsic heights of the tab grid toolbars.
const CGFloat kTabGridTopToolbarHeight = 52.0f;
const CGFloat kTabGridBottomToolbarHeight = 44.0f;

// The distance travelled by the thumb strip thumbnails during the slide-in
// animation of the thumb strip reveal transition.
const CGFloat kThumbStripSlideInHeight = 40.0f;
// The distance travelled by the thumb strip's plus sign button during the
// slide-out animation of the transition from Peeked to Revealed state.
const CGFloat kThumbStripPlusSignButtonSlideOutDistance = 400.0f;

// Alpha of the background color of the toolbar.
const CGFloat kToolbarBackgroundAlpha = 0.75;
