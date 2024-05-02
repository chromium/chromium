// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

namespace base {
class TimeDelta;
}  // namespace base

// Keys of UMA IOS.TabSwitcher.Idle histograms.
extern const char kUMATabSwitcherIdleIncognitoTabGridPageHistogram[];
extern const char kUMATabSwitcherIdleRegularTabGridPageHistogram[];
extern const char kUMATabSwitcherIdleRecentTabsHistogram[];
extern const char kUMATabSwitcherIdleTabGroupsHistogram[];

// Accessibility identifiers for automated testing.
extern NSString* const kTabGridIncognitoTabsPageButtonIdentifier;
extern NSString* const kTabGridRegularTabsPageButtonIdentifier;
extern NSString* const kTabGridRemoteTabsPageButtonIdentifier;
extern NSString* const kTabGridTabGroupsPageButtonIdentifier;
extern NSString* const kTabGridDoneButtonIdentifier;
extern NSString* const kTabGridSearchButtonIdentifier;
extern NSString* const kTabGridCancelButtonIdentifier;
extern NSString* const kTabGridCloseAllButtonIdentifier;
extern NSString* const kTabGridUndoCloseAllButtonIdentifier;
extern NSString* const kTabGridIncognitoTabsEmptyStateIdentifier;
extern NSString* const kTabGridRegularTabsEmptyStateIdentifier;
extern NSString* const kTabGridScrollViewIdentifier;
extern NSString* const kRegularTabGridIdentifier;
extern NSString* const kIncognitoTabGridIdentifier;
extern NSString* const kInactiveTabGridIdentifier;
extern NSString* const kInactiveTabGridCloseAllButtonIdentifier;

extern NSString* const kTabGridEditButtonIdentifier;
extern NSString* const kTabGridEditCloseTabsButtonIdentifier;
extern NSString* const kTabGridEditSelectAllButtonIdentifier;
extern NSString* const kTabGridEditAddToButtonIdentifier;
extern NSString* const kTabGridEditShareButtonIdentifier;
extern NSString* const kTabGridSearchBarIdentifier;
extern NSString* const kTabGridSearchTextFieldIdentifierPrefix;
extern NSString* const kTabGridScrimIdentifier;

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// The color of the text buttons in the toolbars.
extern const int kTabGridToolbarTextButtonColor;

// Colors for the empty state.
extern const int kTabGridEmptyStateTitleTextColor;
extern const int kTabGridEmptyStateBodyTextColor;

// The distance the toolbar content is inset from either side.
extern const CGFloat kTabGridToolbarHorizontalInset;

// The distance between the title and body of the empty state view.
extern const CGFloat kTabGridEmptyStateVerticalMargin;

// The insets from the edges for empty state.
extern const CGFloat kTabGridEmptyStateVerticalInset;
extern const CGFloat kTabGridEmptyStateHorizontalInset;

// The insets from the edges for the floating button.
extern const CGFloat kTabGridFloatingButtonVerticalInset;
extern const CGFloat kTabGridFloatingButtonHorizontalInset;

// Intrinsic heights of the tab grid toolbars.
extern const CGFloat kTabGridTopToolbarHeight;
extern const CGFloat kTabGridBottomToolbarHeight;

// The Search bar original width ratio before any width modifiers.
extern const CGFloat kTabGridSearchBarWidthRatio;
// The tab grid Search bar height.
extern const CGFloat kTabGridSearchBarHeight;
// The Search bar width ratio modifier for non-compact orientation.
extern const CGFloat kTabGridSearchBarNonCompactWidthRatioModifier;

// Alpha of the background color of the toolbar.
extern const CGFloat kToolbarBackgroundAlpha;

// Duration for animations in the tab grid.
extern const base::TimeDelta kAnimationDuration;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_H_
