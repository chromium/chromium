// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/tabs/tab_strip_placeholder_view.h"

#include <algorithm>

#import <QuartzCore/QuartzCore.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Returns the animation drag coefficient set by the iPhone simulator.
// This is useful when debugging with the simulator because when
// "slow animation" mode is toggled, it only impacts UIKit animations not
// CoreAnimation animations. By reading the value of that
// UIAnimatonDragCoefficient API we can also slow down Core Animation animations
// with the right value.
// On device we always return 1.0 because it's using private API.
#if TARGET_IPHONE_SIMULATOR
UIKIT_EXTERN float UIAnimationDragCoefficient();
float animationDragCoefficient() {
  return UIAnimationDragCoefficient();
}
#else
float animationDragCoefficient() {
  return 1.0f;
}
#endif

@interface TabStripPlaceholderView () {
  // YES when the fold animation is currently playing.
  BOOL _animatingFold;
}

// Adds a transform animation to |layer| with |duration|, |beginTime|,
// from value |from| to value |to|. The animation will be reversed if |reverse|
// is set to true.
- (void)addTransformAnimationToLayer:(CALayer*)layer
                            duration:(CFTimeInterval)duration
                           beginTime:(CFTimeInterval)beginTime
                                from:(CATransform3D)from
                                  to:(CATransform3D)to
                             reverse:(BOOL)reverse;

// Triggers a fold animation if |fold| is true and an unfold aniamtion if |fold|
// is false. The |completion| block is called at the end of the animation.
- (void)animateFold:(BOOL)fold withCompletion:(ProceduralBlock)completion;

@end

namespace {
// Tab strip fold animation total duration in seconds.
const CGFloat kFoldAnimationDuration = 0.25;
// Tab strip fold animation duration in seconds for a single tab view.
const CGFloat kTabFoldAnimationDuration = 0.15;
}

@implementation TabStripPlaceholderView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    if (UseRTLLayout())
      [self setTransform:CGAffineTransformMakeScale(-1, 1)];
  }
  return self;
}

- (void)foldWithCompletion:(ProceduralBlock)completion {
  [self animateFold:YES withCompletion:completion];
}

- (void)unfoldWithCompletion:(ProceduralBlock)completion {
  [self animateFold:NO withCompletion:completion];
}

#pragma mark - Private

- (void)addTransformAnimationToLayer:(CALayer*)layer
                            duration:(CFTimeInterval)duration
                           beginTime:(CFTimeInterval)beginTime
                                from:(CATransform3D)from
                                  to:(CATransform3D)to
                             reverse:(BOOL)reverse {
  CABasicAnimation* transformAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  transformAnimation.duration = animationDragCoefficient() * duration;
  transformAnimation.beginTime = beginTime;
  transformAnimation.fromValue =
      [NSValue valueWithCATransform3D:(reverse ? to : from)];
  transformAnimation.toValue =
      [NSValue valueWithCATransform3D:(reverse ? from : to)];
  transformAnimation.fillMode = kCAFillModeBoth;
  transformAnimation.removedOnCompletion = NO;
  [layer addAnimation:transformAnimation forKey:@"transform"];
}

- (void)animateFold:(BOOL)fold withCompletion:(ProceduralBlock)completion {
  DCHECK(!_animatingFold);
  const BOOL reversed = !fold;
  _animatingFold = YES;
  [self setUserInteractionEnabled:NO];
  [CATransaction begin];
  __weak TabStripPlaceholderView* weakSelf = self;
  [CATransaction setCompletionBlock:^{
    TabStripPlaceholderView* strongSelf = weakSelf;
    if (!strongSelf) {
      if (completion)
        completion();
      return;
    }
    strongSelf->_animatingFold = NO;
    [strongSelf setUserInteractionEnabled:YES];
    if (completion)
      completion();
    for (UIView* view in [strongSelf subviews]) {
      [view.layer removeAnimationForKey:@"transform"];
    }
  }];
  const CFTimeInterval currentTime =
      [self.layer convertTime:CACurrentMediaTime() fromLayer:nil];
  const NSInteger tabCount = [[self subviews] count];
  const CFTimeInterval deltaTimeBetweenAnimations =
      std::min((kFoldAnimationDuration - kTabFoldAnimationDuration) / tabCount,
               kTabFoldAnimationDuration);
  const BOOL isRTL = UseRTLLayout();
  NSArray* orderedViews = [[self subviews]
      sortedArrayUsingComparator:^(UIView* view1, UIView* view2) {
        if (!isRTL) {
          if (CGRectGetMinX(view1.frame) < CGRectGetMinX(view2.frame))
            return NSOrderedDescending;
          else
            return NSOrderedAscending;
        } else {
          if (CGRectGetMaxX(view1.frame) > CGRectGetMaxX(view2.frame))
            return NSOrderedDescending;
          else
            return NSOrderedAscending;
        }
      }];
  // Fold all subviews one by one with a small delay and following the current
  // UI layout direction.
  int index = 0;
  for (UIView* view in orderedViews) {
    CATransform3D transform = CATransform3DConcat(
        view.layer.transform,
        CATransform3DMakeTranslation(0, view.bounds.size.height, 0));
    [self addTransformAnimationToLayer:view.layer
                              duration:kTabFoldAnimationDuration
                             beginTime:currentTime +
                                       index * deltaTimeBetweenAnimations
                                  from:view.layer.transform
                                    to:transform
                               reverse:reversed];
    ++index;
  }
  [CATransaction commit];
}

@end
