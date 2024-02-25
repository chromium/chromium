// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_FAKE_TAB_GRID_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_FAKE_TAB_GRID_TRANSITION_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"

// Fake transition animation.
@interface FakeTransitionAnimation : NSObject <TabGridTransitionAnimation>

// Count the number of times the `animateWithCompletion:` method is called.
@property(nonatomic, assign) NSUInteger animationCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_FAKE_TAB_GRID_TRANSITION_ANIMATION_H_
