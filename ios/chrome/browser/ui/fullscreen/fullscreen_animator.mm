// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"

#import <math.h>
#import <algorithm>
#import <memory>

#import "base/check_op.h"
#import "ios/chrome/common/material_timing.h"
#import "ui/gfx/geometry/cubic_bezier.h"

CGFloat GetFinalFullscreenProgressForAnimation(FullscreenAnimatorStyle style) {
  return style == FullscreenAnimatorStyle::ENTER_FULLSCREEN ? 0.0 : 1.0;
}

@interface FullscreenAnimator () {
  // The bezier backing the timing curve.
  std::unique_ptr<gfx::CubicBezier> _bezier;
  // The current progress value that was recorded when the animator was stopped.
  CGFloat _progressUponStopping;
}
@end

@implementation FullscreenAnimator
@synthesize style = _style;
@synthesize startProgress = _startProgress;
@synthesize finalProgress = _finalProgress;

- (instancetype)initWithStartProgress:(CGFloat)startProgress
                                style:(FullscreenAnimatorStyle)style {
  // Control points for Material Design CurveEaseOut curve.
  UICubicTimingParameters* timingParams = [[UICubicTimingParameters alloc]
      initWithControlPoint1:CGPointMake(0.0, 0.0)
              controlPoint2:CGPointMake(0.2, 0.1)];
  DCHECK_GE(startProgress, 0.0);
  DCHECK_LE(startProgress, 1.0);
  self = [super initWithDuration:kMaterialDuration1
                timingParameters:timingParams];
  if (self) {
    DCHECK_GE(startProgress, 0.0);
    DCHECK_LE(startProgress, 1.0);
    _style = style;
    _startProgress = startProgress;
    _finalProgress = GetFinalFullscreenProgressForAnimation(_style);
    _bezier = std::make_unique<gfx::CubicBezier>(
        timingParams.controlPoint1.x, timingParams.controlPoint1.y,
        timingParams.controlPoint2.x, timingParams.controlPoint2.y);
  }
  return self;
}

#pragma mark Accessors

- (CGFloat)currentProgress {
  if (self.state == UIViewAnimatingStateStopped)
    return _progressUponStopping;
  CGFloat interpolationFraction = _bezier->Solve(self.fractionComplete);
  CGFloat range = self.finalProgress - self.startProgress;
  return self.startProgress + interpolationFraction * range;
}

#pragma mark Public

- (CGFloat)progressForAnimatingPosition:(UIViewAnimatingPosition)position {
  switch (position) {
    case UIViewAnimatingPositionStart:
      return self.startProgress;
    case UIViewAnimatingPositionEnd:
      return self.finalProgress;
    case UIViewAnimatingPositionCurrent:
      return self.currentProgress;
  }
}

#pragma mark UIViewAnimating

- (void)stopAnimation:(BOOL)withoutFinishing {
  // Record the progress value when transitioning from the active to stopped
  // state.  This allows `currentProgress` to return the correct value after
  // stopping, as `fractionComplete` is reset to 0.0 for stopped animators.
  if (self.state == UIViewAnimatingStateActive)
    _progressUponStopping = self.currentProgress;
  if (_progressUponStopping == _startProgress)
    return;
  [super stopAnimation:withoutFinishing];
}

#pragma mark UIViewPropertyAnimator

- (BOOL)isInterruptible {
  return YES;
}

@end
