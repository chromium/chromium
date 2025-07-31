// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TABS_CLOSURE_ANIMATION_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TABS_CLOSURE_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Available types of tab closure animation.
enum class TabsClosureAnimationType {
  // Grid cells are hidden.
  kHideGridCells,
  // Grid cells are revealed.
  kRevealGridCells,
};

// Creates and triggers the tab closure animation.
@interface TabsClosureAnimation : NSObject

// Type of animation. Defaults to `kHideGridCells`.
@property(nonatomic, assign) TabsClosureAnimationType type;
// Start point of animation in unit coordinate space. Defaults to (0.5, 1.0).
@property(nonatomic, assign) CGPoint startPoint;

- (instancetype)initWithWindow:(UIView*)window
                     gridCells:(NSArray<UIView*>*)gridCells
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Animates the tabs' closure in `gridCells` with a "wipe" effect on top of
// `window`.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TABS_CLOSURE_ANIMATION_H_
