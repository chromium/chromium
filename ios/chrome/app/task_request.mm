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

- (instancetype)initWithSceneID:(std::string_view)sceneID
                   executeBlock:(ProceduralBlock)executeBlock {
  if ((self = [super initWithSceneID:sceneID])) {
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
  std::string _sceneSessionID;
  BOOL _isColdStart;
  __weak SceneState* _sceneState;
}
@end

@implementation TaskRequest

@synthesize minimumStage = _minimumStage;
@synthesize gaiaID = _gaiaID;

- (std::string_view)sceneSessionID {
  return _sceneSessionID;
}

+ (instancetype)taskForURLContext:(UIOpenURLContext*)URLContext
                       sceneState:(SceneState*)sceneState
                      isColdStart:(BOOL)isColdStart {
  return [[TaskRequestForURLContext alloc] initWithURLContext:URLContext
                                                   sceneState:sceneState
                                                  isColdStart:isColdStart];
}

+ (instancetype)taskForUserActivity:(NSUserActivity*)userActivity
                         sceneState:(SceneState*)sceneState
                        isColdStart:(BOOL)isColdStart {
  return [[TaskRequestForUserActivity alloc] initWithUserActivity:userActivity
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
+ (instancetype)taskForTestingWithSceneID:(std::string_view)sceneID
                             executeBlock:(ProceduralBlock)block {
  return [[TaskRequestForTesting alloc] initWithSceneID:sceneID
                                           executeBlock:block];
}

- (instancetype)initWithSceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _sceneState = sceneState;
    _sceneSessionID = sceneState.sceneSessionID;
    _isColdStart = isColdStart;
    // TODO(crbug.com/462018636): Minimum stage can be different in some cases,
    // handle all scenarios based on the received options (check bookmarks
    // feature).
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  }
  return self;
}

// Initializer used for tests.
- (instancetype)initWithSceneID:(std::string_view)sceneID {
  if ((self = [super init])) {
    _sceneSessionID = sceneID;
  }
  return self;
}

- (void)execute {
  NOTREACHED();
}

#pragma mark - Protected

- (SceneState*)sceneStateFromSessionID {
  if (_sceneState && _sceneState.sceneSessionID == _sceneSessionID) {
    return _sceneState;
  }

  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    SceneDelegate* sceneDelegate =
        base::apple::ObjCCast<SceneDelegate>(scene.delegate);
    if (sceneDelegate &&
        sceneDelegate.sceneState.sceneSessionID == _sceneSessionID) {
      _sceneState = sceneDelegate.sceneState;
      return _sceneState;
    }
  }
  NOTREACHED();
}

@end
