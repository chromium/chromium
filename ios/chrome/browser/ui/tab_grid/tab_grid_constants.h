// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifiers for automated testing.
extern NSString* const kTabGridIncognitoTabsPageButtonIdentifier;
extern NSString* const kTabGridRegularTabsPageButtonIdentifier;
extern NSString* const kTabGridRemoteTabsPageButtonIdentifier;
extern NSString* const kTabGridDoneButtonIdentifier;
extern NSString* const kTabGridCloseAllButtonIdentifier;
extern NSString* const kTabGridUndoCloseAllButtonIdentifier;
extern NSString* const kTabGridIncognitoTabsEmptyStateIdentifier;
extern NSString* const kTabGridRegularTabsEmptyStateIdentifier;
extern NSString* const kTabGridScrollViewIdentifier;

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
extern const CGFloat kTabGridFloatingButtonVerticalInsetSmall;
extern const CGFloat kTabGridFloatingButtonVerticalInsetLarge;
extern const CGFloat kTabGridFloatingButtonHorizontalInset;

// Intrinsic heights of the tab grid toolbars.
extern const CGFloat kTabGridTopToolbarHeight;
extern const CGFloat kTabGridBottomToolbarHeight;

// The delay (in milliseconds) after closing the last incognito tab and before
// automatically scrolling to the regular tabs panel.
extern const int64_t kTabGridScrollAnimationDelayInMilliseconds;

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSTANTS_H_
