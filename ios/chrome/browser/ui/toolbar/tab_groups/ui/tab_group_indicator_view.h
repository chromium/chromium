// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_consumer.h"

@protocol TabGroupIndicatorMutator;

// UIView that contains information about the current tab group.
@interface TabGroupIndicatorView : UIView <TabGroupIndicatorConsumer>

// Mutator for actions happening in the view.
@property(nonatomic, weak) id<TabGroupIndicatorMutator> mutator;

// Tracks if the view is available.
@property(nonatomic, assign) BOOL available;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_VIEW_H_
