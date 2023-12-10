// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation_group.h"

#import "base/check.h"

@implementation TabGridTransitionAnimationGroup {
  TabGridTransitionAnimationGroupType _type;
  NSArray<id<TabGridTransitionAnimation>>* _animations;
  NSUInteger _animationsCompleted;
}

#pragma mark - Public

- (instancetype)initWithType:(TabGridTransitionAnimationGroupType)type
                  animations:
                      (NSArray<id<TabGridTransitionAnimation>>*)animations {
  self = [super init];
  if (self) {
    _type = type;
    _animations = [animations copy];
  }
  return self;
}

- (instancetype)initWithAnimations:
    (NSArray<id<TabGridTransitionAnimation>>*)animations {
  return [self initWithType:TabGridTransitionAnimationGroupType::kSerial
                 animations:animations];
}

#pragma mark - TabGridTransitionAnimation

- (void)animateWithCompletion:(ProceduralBlock)completion {
  if (_animations.count == 0) {
    if (completion) {
      completion();
    }
    return;
  }

  _animationsCompleted = 0;

  switch (_type) {
    case TabGridTransitionAnimationGroupType::kSerial:
      [self startSerialAnimationWithCompletion:completion];
      break;
    case TabGridTransitionAnimationGroupType::kConcurrent:
      [self startConcurentAnimationWithCompletion:completion];
      break;
  }
}

#pragma mark - Private

// Performs the animations one after the other and executes the `completion`
// handler.
- (void)startSerialAnimationWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  CHECK(_animations.count);
  [_animations[0] animateWithCompletion:^{
    if (weakSelf) {
      [weakSelf loopOnSerialAnimationWithCompletion:completion];
      return;
    }
    if (completion) {
      completion();
    }
  }];
}

// Loops on the animations and excecutes the `completion` handler.
- (void)loopOnSerialAnimationWithCompletion:(ProceduralBlock)completion {
  _animationsCompleted++;
  if (_animationsCompleted >= _animations.count) {
    if (completion) {
      completion();
    }
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [_animations[_animationsCompleted] animateWithCompletion:^{
    if (weakSelf) {
      [weakSelf loopOnSerialAnimationWithCompletion:completion];
      return;
    }
    if (completion) {
      completion();
    }
  }];
}

// Performs the animations at the same time and executes the `completion`
// handler.
- (void)startConcurentAnimationWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  for (id<TabGridTransitionAnimation> animation in _animations) {
    [animation animateWithCompletion:^{
      if (weakSelf) {
        [self concurrentAnimationCompletionBlock:completion];
        return;
      }
      if (completion) {
        completion();
      }
    }];
  }
}

// Performs the `completion` handler if the animation is the last one.
- (void)concurrentAnimationCompletionBlock:(ProceduralBlock)completion {
  _animationsCompleted++;
  if (_animationsCompleted >= _animations.count && completion) {
    completion();
  }
}

@end
