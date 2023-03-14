// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_REVERSED_ANIMATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_REVERSED_ANIMATION_H_

#import <UIKit/UIKit.h>

// Returns an animation that reverses `animation` when added to `layer`.
CAAnimation* CAAnimationMakeReverse(CAAnimation* animation, CALayer* layer);

// Removes the animation for `key` from each CALayer in `layers`, creates
// reversed versions using `CAAnimationMakeReverse`, then adds the reversed
// animation back to the layers under the same key.
void ReverseAnimationsForKeyForLayers(NSString* key, NSArray* layers);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_REVERSED_ANIMATION_H_
