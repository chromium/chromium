// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Height of the primary tab group indicator view.
extern const CGFloat kTabGroupIndicatorHeight;
// Vertical margin for the primary tab group indicator view.
extern const CGFloat kTabGroupIndicatorVerticalMargin;
// Size of the tab group indicator view colored dot.
extern const CGFloat kTabGroupIndicatorColoredDotSize;
// Margin between the tab group indicator view colored dot and label.
extern const CGFloat kTabGroupIndicatorSeparationMargin;

// Top margin for the NTP tab group indicator view.
extern const CGFloat kTabGroupIndicatorNTPTopMargin;
// Margin between the NTP tab group indicator and the toolbar.
extern const CGFloat kTabGroupIndicatorNTPToolbarMargin;

// Accessibility identifier for the tab group indicator view.
extern NSString* const kTabGroupIndicatorViewIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSTANTS_H_
