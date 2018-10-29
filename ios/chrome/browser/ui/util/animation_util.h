// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_ANIMATION_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_ANIMATION_UTIL_H_

#import <UIKit/UIKit.h>

// Animations returned by these utility methods use kCAFillModeBoth and aren't
// automatically removed from the layer when finished.  Remove the animations
// by calling |RemoveAnimationsFromLayers| in CATransaction completion blocks.
// This is done so the effects of animations that finish earlier persist until
// all animations in the transaction are finished.

// Returns an animation that will animate |layer| from |beginFrame| to
// |endFrame|.
CAAnimation* FrameAnimationMake(CALayer* layer,
                                CGRect beginFrame,
                                CGRect endFrame);

// Returns an animation that will animate the "opacity" property of a layer from
// |beginOpacity| to |endOpacity|.
CAAnimation* OpacityAnimationMake(CGFloat beginOpacity, CGFloat endOpacity);

// Returns an animation group containing the animations in |animations| that has
// the shortest duration necessary for all the animations to finish.
CAAnimation* AnimationGroupMake(NSArray* animations);

// Returns an animation that performs |animation| after |delay|.
CAAnimation* DelayedAnimationMake(CAAnimation* animation, CFTimeInterval delay);

// If |animation| is a CAAnimationGroup, searches its animations for a
// CABasicAnimation for |keyPath|.  If |animation| is a CABasicAnimation, it
// will check its keyPath against |keyPath|.
CABasicAnimation* FindAnimationForKeyPath(NSString* keyPath,
                                          CAAnimation* animation);

// Removes the animation for |key| from each CALayer in |layers|.
void RemoveAnimationForKeyFromLayers(NSString* key, NSArray* layers);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_ANIMATION_UTIL_H_
