// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_consumer.h"

@protocol TabGroupIndicatorMutator;
@protocol ToolbarHeightDelegate;

// UIView that contains information about the current tab group.
@interface TabGroupIndicatorView : UIView <TabGroupIndicatorConsumer>

// Mutator for actions happening in the view.
@property(nonatomic, weak) id<TabGroupIndicatorMutator> mutator;

/// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Tracks if the view is available.
@property(nonatomic, assign) BOOL available;

// Whether the view is displayed in Incognito mode.
@property(nonatomic, assign) BOOL incognito;

// Whether the view is displayed on the NTP.
@property(nonatomic, assign) BOOL displayedOnNTP;

// Whether or not to display the separator at the bottom.
@property(nonatomic, assign) BOOL showSeparator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_
