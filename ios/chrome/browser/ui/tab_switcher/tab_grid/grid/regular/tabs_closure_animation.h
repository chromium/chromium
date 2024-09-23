// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_TABS_CLOSURE_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_TABS_CLOSURE_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Creates and triggers the tab closure animation.
@interface TabsClosureAnimation : NSObject

- (instancetype)initWithWindow:(UIView*)window
                     gridCells:(NSArray<UIView*>*)gridCells
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Animates the tabs' closure in `gridCells` with a "wipe" effect on top of
// `window`.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_TABS_CLOSURE_ANIMATION_H_
