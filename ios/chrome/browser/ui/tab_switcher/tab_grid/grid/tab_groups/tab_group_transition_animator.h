// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animates the overall appearance/disappearance of the TabGroup, *not* the
// animations of the inner elements.
@interface TabGroupTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// Whether the TabGroup is appearing.
@property(nonatomic, assign) BOOL appearing;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_TRANSITION_ANIMATOR_H_
