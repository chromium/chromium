// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/animation_util.h"

#import <algorithm>
#import <cmath>

#import "ios/chrome/browser/shared/ui/util/reversed_animation.h"

CAAnimation* FrameAnimationMake(CALayer* layer,
                                CGRect beginFrame,
                                CGRect endFrame) {
  CGRect beginBounds = {CGPointZero, beginFrame.size};
  CGRect endBounds = {CGPointZero, endFrame.size};
  CABasicAnimation* boundsAnimation =
      [CABasicAnimation animationWithKeyPath:@"bounds"];
  boundsAnimation.fromValue = [NSValue valueWithCGRect:beginBounds];
  boundsAnimation.toValue = [NSValue valueWithCGRect:endBounds];
  boundsAnimation.removedOnCompletion = NO;
  boundsAnimation.fillMode = kCAFillModeBoth;
  CGPoint beginPosition = CGPointMake(
      beginFrame.origin.x + layer.anchorPoint.x * beginBounds.size.width,
      beginFrame.origin.y + layer.anchorPoint.y * beginBounds.size.height);
  CGPoint endPosition = CGPointMake(
      endFrame.origin.x + layer.anchorPoint.x * endBounds.size.width,
      endFrame.origin.y + layer.anchorPoint.y * endBounds.size.height);
  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.fromValue = [NSValue valueWithCGPoint:beginPosition];
  positionAnimation.toValue = [NSValue valueWithCGPoint:endPosition];
  positionAnimation.removedOnCompletion = NO;
  positionAnimation.fillMode = kCAFillModeBoth;
  return AnimationGroupMake(@[ boundsAnimation, positionAnimation ]);
}

CAAnimation* OpacityAnimationMake(CGFloat beginOpacity, CGFloat endOpacity) {
  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.fromValue = @(beginOpacity);
  opacityAnimation.toValue = @(endOpacity);
  opacityAnimation.fillMode = kCAFillModeBoth;
  opacityAnimation.removedOnCompletion = NO;
  return opacityAnimation;
}

CAAnimation* AnimationGroupMake(NSArray* animations) {
  CAAnimationGroup* animationGroup = [CAAnimationGroup animation];
  animationGroup.animations = animations;
  CFTimeInterval duration = 0.0;
  for (CAAnimation* animation in animations) {
    duration = std::max(duration, animation.beginTime + animation.duration);
  }
  animationGroup.duration = duration;
  animationGroup.fillMode = kCAFillModeBoth;
  animationGroup.removedOnCompletion = NO;
  return animationGroup;
}

CAAnimation* DelayedAnimationMake(CAAnimation* animation,
                                  CFTimeInterval delay) {
  CAAnimation* delayedAnimation = [animation copy];
  if (delayedAnimation) {
    delayedAnimation.beginTime = delay;
    delayedAnimation = AnimationGroupMake(@[ delayedAnimation ]);
  }
  return delayedAnimation;
}

CABasicAnimation* FindAnimationForKeyPath(NSString* keyPath,
                                          CAAnimation* animation) {
  __block CABasicAnimation* animationForKeyPath = nil;
  if ([animation isKindOfClass:[CABasicAnimation class]]) {
    CABasicAnimation* basicAnimation =
        static_cast<CABasicAnimation*>(animation);
    if ([basicAnimation.keyPath isEqualToString:keyPath]) {
      animationForKeyPath = basicAnimation;
    }
  } else if ([animation isKindOfClass:[CAAnimationGroup class]]) {
    CAAnimationGroup* animationGroup =
        static_cast<CAAnimationGroup*>(animation);
    [animationGroup.animations
        enumerateObjectsUsingBlock:^(CAAnimation* subAnimation, NSUInteger idx,
                                     BOOL* stop) {
          animationForKeyPath = FindAnimationForKeyPath(keyPath, subAnimation);
          *stop = animationForKeyPath != nil;
        }];
  }
  return animationForKeyPath;
}

void RemoveAnimationForKeyFromLayers(NSString* key, NSArray* layers) {
  for (CALayer* layer in layers) {
    [layer removeAnimationForKey:key];
  }
}
