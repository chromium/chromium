// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_

#import "base/ios/block_types.h"

// Protocol for the tab grid transition animations.
@protocol TabGridTransitionAnimation <NSObject>

// Performs the animation with the given completion block.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_TRANSITION_ANIMATION_H_
