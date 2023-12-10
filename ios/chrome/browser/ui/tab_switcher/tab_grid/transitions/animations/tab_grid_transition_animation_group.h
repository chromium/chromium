// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_GROUP_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_GROUP_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"

// Type of the tab grid transition animation.
enum class TabGridTransitionAnimationGroupType {
  // Each animation is executed one after the other.
  kSerial,
  // Each animation is executed at the same time.
  kConcurrent,
};

// Performs a transition animation that combines a set of transition animations
// from Tab Grid to Browser and vice versa.
@interface TabGridTransitionAnimationGroup
    : NSObject <TabGridTransitionAnimation>

// Designated initializer.
- (instancetype)initWithType:(TabGridTransitionAnimationGroupType)type
                  animations:
                      (NSArray<id<TabGridTransitionAnimation>>*)animations
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer that creates a TabGridTransitionAnimationGroup with
// the `kSerial` TabGridTransitionAnimationGroupType.
- (instancetype)initWithAnimations:
    (NSArray<id<TabGridTransitionAnimation>>*)animations;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_GROUP_H_
