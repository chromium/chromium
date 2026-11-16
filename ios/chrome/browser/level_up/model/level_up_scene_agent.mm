// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_scene_agent.h"

#import <UIKit/UIKit.h>

#import <map>
#import <string>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/level_up/model/level_up_metrics.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The listening scene agent whose window the user touched most recently.
__weak LevelUpSceneAgent* g_last_touched_agent = nil;

}  // namespace

@interface LevelUpSceneAgent () <UIGestureRecognizerDelegate>
@end

@implementation LevelUpSceneAgent {
  // The callback registered with `base::AddActionCallback`.
  base::ActionCallback _actionCallback;
  // The level up service.
  raw_ptr<LevelUpService> _levelUpService;
  // Map from user action to task type for fast lookup.
  std::map<std::string, TaskType> _actionToTaskMap;
  // Map from user action to stat type for fast lookup.
  std::map<std::string, LevelUpTaskStatType> _actionToStatMap;
  // Gesture recognizer that detects when the user touches the window.
  UIGestureRecognizer* _gestureRecognizer;
}

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self handleActivationLevel:sceneState.activationLevel];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self handleActivationLevel:level];
}

// Handles scene activation level changes.
- (void)handleActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    [self startListening];
    [self recordForegroundMetrics];
  } else {
    [self stopListening];
  }
}

// Records Level Up metrics when the scene becomes active in the foreground.
- (void)recordForegroundMetrics {
  if (!_levelUpService || !_levelUpService->IsOptedIn()) {
    return;
  }
  RecordLevelUpCurrentLevel(_levelUpService->GetCurrentLevel());
}

- (void)startListening {
  if (_actionCallback) {
    return;
  }

  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return;
  }
  _levelUpService = LevelUpServiceFactory::GetForProfile(profile);

  _actionToTaskMap.clear();
  for (const auto& [type, info] : _levelUpService->GetTasks()) {
    for (const std::string& action : info->GetTriggerUserActions()) {
      _actionToTaskMap[action] = type;
    }
  }
  _actionToStatMap = _levelUpService->GetStatTriggerUserActions();

  __weak LevelUpSceneAgent* weakSelf = self;
  _actionCallback = base::BindRepeating(
      ^(const std::string& action, base::TimeTicks action_time) {
        [weakSelf onUserAction:action];
      });
  base::AddActionCallback(_actionCallback);
  [self addGestureRecognizerToWindow];
}

- (void)stopListening {
  if (!_actionCallback) {
    return;
  }
  base::RemoveActionCallback(_actionCallback);
  _actionCallback.Reset();
  [self removeGestureRecognizerFromWindow];
  if (g_last_touched_agent == self) {
    g_last_touched_agent = nil;
  }
  _levelUpService = nullptr;
  _actionToTaskMap.clear();
  _actionToStatMap.clear();
}

- (void)dealloc {
  [self stopListening];
}

- (void)onUserAction:(const std::string&)action {
  if (!_levelUpService || !_levelUpService->IsOptedIn()) {
    return;
  }
  if (![self shouldHandleUserActions]) {
    return;
  }

  auto statIt = _actionToStatMap.find(action);
  if (statIt != _actionToStatMap.end()) {
    _levelUpService->IncrementStatValue(statIt->second, 1);
  }

  auto it = _actionToTaskMap.find(action);
  if (it != _actionToTaskMap.end()) {
    TaskType taskType = it->second;
    if (!_levelUpService->IsTaskCompleted(taskType)) {
      _levelUpService->MarkTaskCompleted(taskType);
      if (_levelUpService->IsUIEnabled()) {
        [self showCompletionSnackbarForTask:taskType];
      }
    }
  }
}

// Displays the completion snackbar for the given task.
- (void)showCompletionSnackbarForTask:(TaskType)taskType {
  const TaskInfo* taskInfo = _levelUpService->GetTaskInfo(taskType);
  if (!taskInfo) {
    return;
  }

  std::string message = taskInfo->GetCompletionSnackbarMessage();
  if (message.empty()) {
    return;
  }

  Browser* browser = [self currentBrowser];
  CHECK(browser);

  // By design, no completion snackbar is shown in incognito.
  if (browser->type() == Browser::Type::kIncognito) {
    return;
  }

  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);

  SnackbarMessage* snackbarMessage =
      [[SnackbarMessage alloc] initWithTitle:base::SysUTF8ToNSString(message)];

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SNACKBAR_ACTION);

  __weak LevelUpSceneAgent* weakSelf = self;
  action.handler = ^{
    [weakSelf onSnackbarAction];
  };

  snackbarMessage.action = action;

  [snackbarHandler showSnackbarMessage:snackbarMessage];
}

#pragma mark - Private

// Returns whether this `LevelUpSceneAgent` handles user actions.
- (BOOL)shouldHandleUserActions {
  LevelUpSceneAgent* lastTouchedLevelUpSceneAgent = g_last_touched_agent;
  return !lastTouchedLevelUpSceneAgent || lastTouchedLevelUpSceneAgent == self;
}

// Adds `_gestureRecognizer` to the scene's window, so that touching the window
// makes this `LevelUpSceneAgent` the one that handles user actions.
- (void)addGestureRecognizerToWindow {
  _gestureRecognizer = [[UIGestureRecognizer alloc] initWithTarget:nil
                                                            action:nil];
  _gestureRecognizer.delegate = self;
  [self.sceneState.window addGestureRecognizer:_gestureRecognizer];
}

// Removes `_gestureRecognizer` from the scene's window.
- (void)removeGestureRecognizerFromWindow {
  [_gestureRecognizer.view removeGestureRecognizer:_gestureRecognizer];
  _gestureRecognizer = nil;
}

// Returns the current active browser for the associated scene.
- (Browser*)currentBrowser {
  return self.sceneState.browserProviderInterface.currentBrowser;
}

// Called when the user taps the action button in the completion snackbar.
- (void)onSnackbarAction {
  Browser* browser = [self currentBrowser];
  CHECK(browser);

  // Since there is no active NTP, an NTP will be created before presenting
  // Level Up.
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
  __weak LevelUpSceneAgent* weakSelf = self;
  [sceneHandler prepareToPresentModalWithSnackbarDismissal:NO
                                                completion:^{
                                                  [weakSelf showLevelUp];
                                                }];
}

// Shows the Level Up UI.
- (void)showLevelUp {
  Browser* browser = [self currentBrowser];
  CHECK(browser);

  id<LevelUpCommands> levelUpHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LevelUpCommands);
  [levelUpHandler showLevelUp];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  g_last_touched_agent = self;
  return NO;
}

@end
