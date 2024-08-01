// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_FOREGROUND_TAB_ANIMATION_VIEW_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_FOREGROUND_TAB_ANIMATION_VIEW_H_

#import <UIKit/UIKit.h>

// View used to contain an animation of a new tab opening in the foreground.
// A content subview animates to fill the view while the background fades to
// black.
@interface ForegroundTabAnimationView : UIView

// The content view (typically the new tab's view) to animate.
@property(nonatomic, strong) UIView* contentView;

// Starts a New Tab animation in `parentView`, from `originPoint` with
// a `completion` block. The new tab will scale up and move from the direction
// if `originPoint` to the center of the receiver. `originPoint` must be in
// UIWindow coordinates.
- (void)animateFrom:(CGPoint)originPoint withCompletion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_FOREGROUND_TAB_ANIMATION_VIEW_H_
