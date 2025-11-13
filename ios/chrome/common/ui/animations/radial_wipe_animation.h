// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ANIMATIONS_RADIAL_WIPE_ANIMATION_H_
#define IOS_CHROME_COMMON_UI_ANIMATIONS_RADIAL_WIPE_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Available types of radial wipe animation.
enum class RadialWipeAnimationType {
  // Target views are hidden by the animation.
  kHideTarget,
  // Target views are revealed by the animation.
  kRevealTarget,
};

// Creates and triggers a radial wipe animation.
@interface RadialWipeAnimation : NSObject

// Type of animation. Defaults to `kHideTarget`.
@property(nonatomic, assign) RadialWipeAnimationType type;
// Start point of animation in unit coordinate space. Defaults to (0.5, 1.0).
@property(nonatomic, assign) CGPoint startPoint;

- (instancetype)initWithWindow:(UIView*)window
                   targetViews:(NSArray<UIView*>*)targetViews
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Animates the `targetViews` with a "wipe" effect on top of `window`.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_COMMON_UI_ANIMATIONS_RADIAL_WIPE_ANIMATION_H_
