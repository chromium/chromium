// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_DELEGATE_H_

#import <UIKit/UIKit.h>

@class TabGroupViewController;

// Transition delegate for the TabGroup view, handling all the animation
// configurations.
@interface TabGroupTransitionDelegate
    : NSObject <UIViewControllerTransitioningDelegate>

// Inits with the TabGroup view controller to handle the animation.
- (instancetype)initWithTabGroupViewController:
    (TabGroupViewController*)tabGroupViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_DELEGATE_H_
