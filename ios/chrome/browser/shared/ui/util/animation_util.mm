// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/animation_util.h"

#import <algorithm>

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
