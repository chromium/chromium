// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/change_profile/change_profile_observer.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/ui/authentication/change_profile/change_profile_continuation.h"

@implementation ChangeProfileObserver {
  NSArray<id<ChangeProfileContinuation>>* _continuations;
}

- (instancetype)initWithContinuations:
    (NSArray<id<ChangeProfileContinuation>>*)continuations {
  self = [super init];
  if (self) {
    _continuations = continuations;
  }
  return self;
}

#pragma mark - ChangeProfileObserving

- (void)operationFailed:(ChangeProfileFailure)failure {
  // TODO(crbug.com/375605174): Present a minimalistic dialog in this case.
}

- (void)willStartOperation:(UIViewController*)viewController {
  // Nothing to do.
}

- (void)operationDidComplete:(UIViewController*)viewController
              withSceneState:(SceneState*)sceneState {
  DCHECK(sceneState);
  [self executeContinuationWithIndex:0 sceneState:sceneState];
}

#pragma mark - Private

- (void)executeContinuationWithIndex:(NSUInteger)index
                          sceneState:(SceneState*)sceneState {
  if (index >= _continuations.count) {
    return;
  }

  __weak SceneState* weakSceneState = sceneState;
  [_continuations[index]
      executeWithSceneState:sceneState
                 completion:^{
                   if (weakSceneState) {
                     [self executeContinuationWithIndex:index + 1
                                             sceneState:weakSceneState];
                   }
                 }];
}

@end
