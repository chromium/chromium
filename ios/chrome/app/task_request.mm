// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/task_request_shortcut_item.h"
#import "ios/chrome/app/task_request_url_context.h"
#import "ios/chrome/app/task_request_user_activity.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface TaskRequestForTesting : TaskRequest
@end

@implementation TaskRequestForTesting {
  ProceduralBlock _executeBlock;
}

- (instancetype)initWithScene:(UIScene*)scene
                 executeBlock:(ProceduralBlock)executeBlock {
  if ((self = [super initWithScene:scene])) {
    CHECK(executeBlock);
    _executeBlock = [executeBlock copy];
  }
  return self;
}

- (void)execute {
  _executeBlock();
}

@end

@interface TaskRequest () {
  __weak UIScene* _scene;
  BOOL _isColdStart;
  __weak SceneState* _sceneState;
}
@end

@implementation TaskRequest

@synthesize minimumStage = _minimumStage;
@synthesize gaiaID = _gaiaID;

- (UIScene*)scene {
  return _scene;
}

+ (instancetype)taskForURLContext:(UIOpenURLContext*)URLContext
                       sceneState:(SceneState*)sceneState
                      isColdStart:(BOOL)isColdStart {
  return [TaskRequestForURLContext taskRequestWithURLContext:URLContext
                                                  sceneState:sceneState
                                                 isColdStart:isColdStart];
}

+ (instancetype)taskForUserActivity:(NSUserActivity*)userActivity
                         sceneState:(SceneState*)sceneState
                        isColdStart:(BOOL)isColdStart {
  return [TaskRequestForUserActivity taskRequestWithUserActivity:userActivity
                                                      sceneState:sceneState
                                                     isColdStart:isColdStart];
}

+ (instancetype)taskForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                         sceneState:(SceneState*)sceneState
                            handler:(ShortcutCompletionHandler)handler
                        isColdStart:(BOOL)isColdStart {
  return [[TaskRequestForShortcutItem alloc] initWithShortcutItem:shortcutItem
                                                       sceneState:sceneState
                                                          handler:handler
                                                      isColdStart:isColdStart];
}

// Factory used for tests.
+ (instancetype)taskForTestingWithScene:(UIScene*)scene
                           executeBlock:(ProceduralBlock)block {
  return [[TaskRequestForTesting alloc] initWithScene:scene executeBlock:block];
}

- (instancetype)initWithSceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _sceneState = sceneState;
    _scene = sceneState.scene;
    _isColdStart = isColdStart;
    // TODO(crbug.com/462018636): Minimum stage can be different in some cases,
    // handle all scenarios based on the received options (check bookmarks
    // feature).
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  }
  return self;
}

// Initializer used for tests.
- (instancetype)initWithScene:(UIScene*)scene {
  if ((self = [super init])) {
    _scene = scene;
  }
  return self;
}

- (void)execute {
  NOTREACHED();
}

#pragma mark - Protected

- (SceneState*)sceneState {
  if (!_sceneState) {
    _sceneState =
        base::apple::ObjCCast<SceneDelegate>(_scene.delegate).sceneState;
  }
  CHECK(_sceneState);
  return _sceneState;
}

@end
