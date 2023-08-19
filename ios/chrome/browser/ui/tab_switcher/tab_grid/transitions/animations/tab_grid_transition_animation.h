// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_

#import "base/ios/block_types.h"

// Protocol that defines a transition animation from Tab Grid to Browser and
// vice versa.
@protocol TabGridTransitionAnimation

// Performs the animation with a `completion` handler.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_
