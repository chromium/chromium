// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface TaskRequest () {
  std::string _sceneSessionID;
}

@property(nonatomic, assign) TaskSource source;

@end

@implementation TaskRequest

@synthesize minimumStage = _minimumStage;

- (std::string_view)sceneSessionID {
  return _sceneSessionID;
}

- (instancetype)initWithURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
                         sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = TaskSource::TaskSourceContextURL;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
}

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = TaskSource::TaskSourceUserActivity;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                             handler:(ShortcutCompletionHandler)handler
                          sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = TaskSource::TaskSourceQuickAction;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
}

- (instancetype)initWithConnectionOptions:(UISceneConnectionOptions*)options
                               sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = TaskSource::TaskSourceColdStart;
    // TODO(crbug.com/462018636): Minimum stage can be different in this case,
    // handle all scenarios based on the received options (check bookmarks
    // feature).
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
  }
  return self;
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

- (void)executeShortcutItem {
  // TODO(crbug.com/462018636): Add implementation.
}

- (void)executeUserActivity {
  // TODO(crbug.com/462018636): Add implementation.
}

- (void)executeContextURL {
  // TODO(crbug.com/462018636): Add implementation.
}

@end
