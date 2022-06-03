// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_ANIMATOR_H_

#import "ios/chrome/browser/ui/util/optional_property_animator.h"

// Enum describing whether the animator should enter or exit fullscreen.
enum class FullscreenAnimatorStyle : short {
  ENTER_FULLSCREEN,
  EXIT_FULLSCREEN
};

// Returns the final fullscreen progress for an animation with |style|.
CGFloat GetFinalFullscreenProgressForAnimation(FullscreenAnimatorStyle style);

// Helper object for animating changes to fullscreen progress.  Subclasses of
// this object are provided to FullscreenControllerObservers to coordinate
// animations across several different ojects.
@interface FullscreenAnimator : OptionalPropertyAnimator

// The animator style.
@property(nonatomic, readonly) FullscreenAnimatorStyle style;
// The progress value at the start of the animation.
@property(nonatomic, readonly) CGFloat startProgress;
// The final calculated fullscreen value.
@property(nonatomic, readonly) CGFloat finalProgress;
// The current progress value.  This is the fullscreen progress value
// interpolated between |startProgress| and |finalProgress| using the timing
// curve and the fraction complete of the animation.
@property(nonatomic, readonly) CGFloat currentProgress;

// Designated initializer.
- (instancetype)initWithStartProgress:(CGFloat)startProgress
                                style:(FullscreenAnimatorStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithDuration:(NSTimeInterval)duration
                timingParameters:(id<UITimingCurveProvider>)parameters
    NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Returns the progress value corresponding with |position|.
- (CGFloat)progressForAnimatingPosition:(UIViewAnimatingPosition)position;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_ANIMATOR_H_
