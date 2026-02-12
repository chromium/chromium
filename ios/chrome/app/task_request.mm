// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
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
    _executeBlock = executeBlock;
  }
  return self;
}

- (void)execute {
  _executeBlock();
}

@end

@interface TaskRequest () {
  std::string _sceneSessionID;
}

@property(nonatomic, assign) TaskSource source;

// Properties needed to handle a shortcut item.
@property(nonatomic, copy) ShortcutCompletionHandler shortcutHandler;
@property(nonatomic, strong) UIApplicationShortcutItem* shortcutItem;

@end

@implementation TaskRequest

@synthesize minimumStage = _minimumStage;

- (std::string_view)sceneSessionID {
  return _sceneSessionID;
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                        taskSource:(TaskSource)taskSource {
  self = [super init];
  if (self) {
    // TODO(crbug.com/462018636): Minimum stage can be different in this case,
    // handle all scenarios based on the received options (check bookmarks
    // feature).
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
}

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState
                          taskSource:(TaskSource)taskSource {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                          sceneState:(SceneState*)sceneState
                          taskSource:(TaskSource)taskSource
                             handler:(ShortcutCompletionHandler)handler {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
    _shortcutItem = shortcutItem;
    _shortcutHandler = [handler copy];
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

// Factory used for tests.
+ (instancetype)taskForTestingWithSceneID:(std::string_view)sceneID
                             executeBlock:(ProceduralBlock)block {
  return [[TaskRequestForTesting alloc] initWithSceneID:sceneID
                                           executeBlock:block];
}

- (void)execute {
  switch (_source) {
    case TaskSource::TaskSourceColdStart:
      // TODO(crbug.com/462018636): Handle cold start logic.
      break;
    case TaskSource::TaskSourceContextURL:
      [self executeContextURL];
      break;
    case TaskSource::TaskSourceUserActivity:
      [self executeUserActivity];
      break;
    case TaskSource::TaskSourceQuickAction:
      [self executeShortcutItem];
      break;
  }
}

#pragma mark - Private

// TODO(crbug.com/462018636): Find a better solution to get the SceneState from
// the sceneSessionID.
- (SceneState*)sceneStateFromSessionID {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    SceneDelegate* sceneDelegate =
        base::apple::ObjCCast<SceneDelegate>(scene.delegate);
    if (sceneDelegate &&
        sceneDelegate.sceneState.sceneSessionID == _sceneSessionID) {
      return sceneDelegate.sceneState;
    }
  }
  return nil;
}

- (void)executeShortcutItem {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  BOOL handledShortcutItem =
      userActivityBrowserAgent->Handle3DTouchApplicationShortcuts(
          self.shortcutItem);
  if (_shortcutHandler) {
    _shortcutHandler(handledShortcutItem);
  }
}

- (void)executeUserActivity {
  // TODO(crbug.com/462018636): Add implementation.
}

- (void)executeContextURL {
  // TODO(crbug.com/462018636): Add implementation.
}

@end
