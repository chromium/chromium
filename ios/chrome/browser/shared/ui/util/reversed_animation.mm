// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/reversed_animation.h"

#import <QuartzCore/QuartzCore.h>
#import <algorithm>
#import <cmath>

@protocol ReversedAnimationProtocol;
typedef CAAnimation<ReversedAnimationProtocol> ReversedAnimation;

namespace {
// Enum type used to denote the direction of a reversed animation relative to
// the original animation's direction.
typedef enum {
  ANIMATION_DIRECTION_NORMAL,
  ANIMATION_DIRECTION_REVERSE
} AnimationDirection;
// Returns the AnimationDirection opposite of `direction`.
AnimationDirection AnimationDirectionOpposite(AnimationDirection direction) {
  return direction == ANIMATION_DIRECTION_NORMAL ? ANIMATION_DIRECTION_REVERSE
                                                 : ANIMATION_DIRECTION_NORMAL;
}
}  // namespace

// Returns an animation that reverses `animation` when added to `layer`, given
// that `animation` is in `parent`'s timespace, which begins at
// `parentBeginTime`.
CAAnimation* CAAnimationMakeReverse(CAAnimation* animation,
                                    CALayer* layer,
                                    CAAnimationGroup* parent,
                                    CFTimeInterval parentBeginTime);
// Updates `reversedAnimation`'s properties for `animationToReverse`, given that
// `animationToReverse` is in `parent`'s timespace, which begins at
// `parentBeginTime`.
void UpdateReversedAnimation(ReversedAnimation* reversedAnimation,
                             CAAnimation* animationToReverse,
                             CALayer* layer,
                             CAAnimationGroup* parent,
                             CFTimeInterval parentBeginTime);

#pragma mark - ReversedAnimation protocol

@protocol ReversedAnimationProtocol <NSObject>

// The original animation that's being played in reverse.
@property(nonatomic, retain) CAAnimation* originalAnimation;
// The current direction for the animation.
@property(nonatomic, assign) AnimationDirection animationDirection;
// The offset into the original animation's duration at the beginning of the
// reverse animation.
@property(nonatomic, assign) CFTimeInterval animationTimeOffset;

@end

#pragma mark - ReversedBasicAnimation

@interface ReversedBasicAnimation : CABasicAnimation <ReversedAnimationProtocol>

// Returns an animation that performs `animation` in reverse when added to
// `layer`.  `parentBeginTime` should be set to the beginTime in absolute time
// of `animation`'s parent in the timing hierarchy.
+ (instancetype)reversedAnimationForAnimation:(CABasicAnimation*)animation
                                     forLayer:(CALayer*)layer
                                       parent:(CAAnimationGroup*)parent
                              parentBeginTime:(CFTimeInterval)parentBeginTime;

@end

@implementation ReversedBasicAnimation

@synthesize originalAnimation = _originalAnimation;
@synthesize animationDirection = _animationDirection;
@synthesize animationTimeOffset = _animationTimeOffset;

- (instancetype)copyWithZone:(NSZone*)zone {
  ReversedBasicAnimation* copy = [super copyWithZone:zone];
  copy.originalAnimation = self.originalAnimation;
  copy.animationDirection = self.animationDirection;
  copy.animationTimeOffset = self.animationTimeOffset;
  return copy;
}

+ (instancetype)reversedAnimationForAnimation:(CABasicAnimation*)animation
                                     forLayer:(CALayer*)layer
                                       parent:(CAAnimationGroup*)parent
                              parentBeginTime:(CFTimeInterval)parentBeginTime {
  // Create new animation and copy properties.  Note that we can't use `-copy`
  // because we need the new animation to be the correct class.
  NSString* keyPath = animation.keyPath;
  CFTimeInterval now =
      [layer convertTime:CACurrentMediaTime() fromLayer:nil] - parentBeginTime;
  ReversedBasicAnimation* reversedAnimation =
      [ReversedBasicAnimation animationWithKeyPath:keyPath];
  UpdateReversedAnimation(reversedAnimation, animation, layer, parent,
                          parentBeginTime);

  // Update from and to values.
  BOOL isReversed =
      reversedAnimation.animationDirection == ANIMATION_DIRECTION_REVERSE;
  CABasicAnimation* originalBasicAnimation =
      static_cast<CABasicAnimation*>(reversedAnimation.originalAnimation);
  reversedAnimation.toValue = isReversed ? originalBasicAnimation.fromValue
                                         : originalBasicAnimation.toValue;
  if (now > animation.beginTime &&
      now < animation.beginTime + animation.duration) {
    // Use the presentation layer's current value for reversals that occur mid-
    // animation.
    reversedAnimation.fromValue =
        [[layer presentationLayer] valueForKeyPath:keyPath];
  } else {
    reversedAnimation.fromValue = isReversed ? originalBasicAnimation.toValue
                                             : originalBasicAnimation.fromValue;
  }
  return reversedAnimation;
}

@end

#pragma mark - ReversedAnimationGroup

@interface ReversedAnimationGroup : CAAnimationGroup <ReversedAnimationProtocol>

// Returns an animation that performs `animation` in reverse when added to
// `layer`.  `parentBeginTime` should be set to the beginTime in absolute time
// of the animation group to which `animation` belongs.
+ (instancetype)reversedAnimationGroupForGroup:(CAAnimationGroup*)group
                                      forLayer:(CALayer*)layer
                                        parent:(CAAnimationGroup*)parent
                               parentBeginTime:(CFTimeInterval)parentBeginTime;

@end

@implementation ReversedAnimationGroup

@synthesize originalAnimation = _originalAnimation;
@synthesize animationDirection = _animationDirection;
@synthesize animationTimeOffset = _animationTimeOffset;

- (instancetype)copyWithZone:(NSZone*)zone {
  ReversedAnimationGroup* copy = [super copyWithZone:zone];
  copy.originalAnimation = self.originalAnimation;
  copy.animationDirection = self.animationDirection;
  copy.animationTimeOffset = self.animationTimeOffset;
  return copy;
}

+ (instancetype)reversedAnimationGroupForGroup:(CAAnimationGroup*)group
                                      forLayer:(CALayer*)layer
                                        parent:(CAAnimationGroup*)parent
                               parentBeginTime:(CFTimeInterval)parentBeginTime {
  // Create new animation and copy properties.  Note that we can't use `-copy`
  // because we need the new animation to be the correct class.
  ReversedAnimationGroup* reversedGroup = [ReversedAnimationGroup animation];
  UpdateReversedAnimation(reversedGroup, group, layer, parent, parentBeginTime);

  // Reverse the animations of the original group.
  NSMutableArray* reversedAnimations = [NSMutableArray array];
  for (CAAnimation* animation in group.animations) {
    CAAnimation* reversedAnimation = CAAnimationMakeReverse(
        animation, layer, group, group.beginTime + parentBeginTime);
    [reversedAnimations addObject:reversedAnimation];
  }
  reversedGroup.animations = reversedAnimations;

  return reversedGroup;
}

@end

#pragma mark - animation_util functions

CAAnimation* CAAnimationMakeReverse(CAAnimation* animation, CALayer* layer) {
  return CAAnimationMakeReverse(animation, layer, nil, layer.beginTime);
}

CAAnimation* CAAnimationMakeReverse(CAAnimation* animation,
                                    CALayer* layer,
                                    CAAnimationGroup* parent,
                                    CFTimeInterval parentBeginTime) {
  CAAnimation* reversedAnimation = nil;
  if ([animation isKindOfClass:[CABasicAnimation class]]) {
    CABasicAnimation* basicAnimation =
        static_cast<CABasicAnimation*>(animation);
    reversedAnimation =
        [ReversedBasicAnimation reversedAnimationForAnimation:basicAnimation
                                                     forLayer:layer
                                                       parent:parent
                                              parentBeginTime:parentBeginTime];
  } else if ([animation isKindOfClass:[CAAnimationGroup class]]) {
    CAAnimationGroup* animationGroup =
        static_cast<CAAnimationGroup*>(animation);
    reversedAnimation =
        [ReversedAnimationGroup reversedAnimationGroupForGroup:animationGroup
                                                      forLayer:layer
                                                        parent:parent
                                               parentBeginTime:parentBeginTime];
  } else {
    // TODO(crbug.com/41211316): Investigate possible general-case reversals. It
    // may be possible to implement this by manipulating the CAMediaTiming
    // properties.
  }
  return reversedAnimation;
}

void UpdateReversedAnimation(ReversedAnimation* reversedAnimation,
                             CAAnimation* animationToReverse,
                             CALayer* layer,
                             CAAnimationGroup* parent,
                             CFTimeInterval parentBeginTime) {
  // Copy properties.
  CFTimeInterval now =
      [layer convertTime:CACurrentMediaTime() fromLayer:nil] - parentBeginTime;
  reversedAnimation.fillMode = animationToReverse.fillMode;
  reversedAnimation.removedOnCompletion =
      animationToReverse.removedOnCompletion;
  reversedAnimation.timingFunction = animationToReverse.timingFunction;

  // Extract the previous reversal if it exists.
  ReversedAnimation* previousReversedAnimation = nil;
  Protocol* reversedAnimationProtocol = @protocol(ReversedAnimationProtocol);
  if ([animationToReverse conformsToProtocol:reversedAnimationProtocol]) {
    previousReversedAnimation =
        static_cast<ReversedAnimation*>(animationToReverse);
    animationToReverse = previousReversedAnimation.originalAnimation;
  }
  reversedAnimation.originalAnimation = animationToReverse;
  reversedAnimation.animationDirection =
      previousReversedAnimation
          ? AnimationDirectionOpposite(
                previousReversedAnimation.animationDirection)
          : ANIMATION_DIRECTION_REVERSE;

  CAAnimation* previousAnimation = previousReversedAnimation
                                       ? previousReversedAnimation
                                       : animationToReverse;
  BOOL isReversed =
      reversedAnimation.animationDirection == ANIMATION_DIRECTION_REVERSE;
  if (now < previousAnimation.beginTime) {
    // Reversal occurs before previous animation begins.
    reversedAnimation.beginTime = 2.0 * now - previousAnimation.beginTime -
                                  reversedAnimation.originalAnimation.duration;
    reversedAnimation.duration = reversedAnimation.originalAnimation.duration;
    reversedAnimation.animationTimeOffset =
        isReversed ? 0 : reversedAnimation.originalAnimation.duration;
  } else if (now < previousAnimation.beginTime + previousAnimation.duration) {
    // Reversal occurs while the previous animation is occurring.
    reversedAnimation.beginTime = 0;
    CFTimeInterval timeDelta = now - previousAnimation.beginTime;
    reversedAnimation.animationTimeOffset =
        previousReversedAnimation.animationTimeOffset +
        (isReversed ? 1.0 : -1.0) * timeDelta;
    reversedAnimation.duration =
        isReversed ? reversedAnimation.animationTimeOffset
                   : reversedAnimation.originalAnimation.duration -
                         reversedAnimation.animationTimeOffset;
  } else {
    // Reversal occurs after the previous animation has ended.
    if (!parentBeginTime) {
      // If the parent's begin time is zero, the animation is using absolute
      // time as its beginTime.
      reversedAnimation.beginTime =
          2.0 * now - previousAnimation.beginTime - previousAnimation.duration;
    } else {
      CFTimeInterval previousEndTime =
          previousAnimation.beginTime + previousAnimation.duration;
      if (now > parent.duration) {
        // The animation's parent has already ended, so use the difference
        // between the parent's ending and the previous animation's end.
        reversedAnimation.beginTime = parent.duration - previousEndTime;
      } else {
        // The parent hasn't ended, so use the difference between the current
        // time and the previous animation's end.
        reversedAnimation.beginTime = now - previousEndTime;
      }
    }
    reversedAnimation.duration = reversedAnimation.originalAnimation.duration;
    reversedAnimation.animationTimeOffset =
        isReversed ? reversedAnimation.originalAnimation.duration : 0;
  }
}

void ReverseAnimationsForKeyForLayers(NSString* key, NSArray* layers) {
  for (CALayer* layer in layers) {
    CAAnimation* reversedAnimation =
        CAAnimationMakeReverse([layer animationForKey:key], layer);
    [layer removeAnimationForKey:key];
    [layer addAnimation:reversedAnimation forKey:key];
  }
}
