// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_scene_agent.h"

#import <map>

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpSceneAgent {
  // The callback registered with `base::AddActionCallback`.
  base::ActionCallback _actionCallback;
  // The level up service.
  raw_ptr<LevelUpService> _levelUpService;
  // Map from user action to task type for fast lookup.
  std::map<std::string, TaskType> _actionToTaskMap;
  // Map from user action to stat type for fast lookup.
  std::map<std::string, LevelUpTaskStatType> _actionToStatMap;
}

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    [self startListening];
  }
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    [self startListening];
  } else {
    [self stopListening];
  }
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
    _actionToTaskMap[info->GetTriggerUserAction()] = type;
  }
  _actionToStatMap = _levelUpService->GetStatTriggerUserActions();

  __weak LevelUpSceneAgent* weakSelf = self;
  _actionCallback = base::BindRepeating(
      ^(const std::string& action, base::TimeTicks action_time) {
        [weakSelf onUserAction:action];
      });
  base::AddActionCallback(_actionCallback);
}

- (void)stopListening {
  if (!_actionCallback) {
    return;
  }
  base::RemoveActionCallback(_actionCallback);
  _actionCallback.Reset();
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

  Browser* browser =
      self.sceneState.browserProviderInterface.currentBrowserProvider.browser;
  if (!browser) {
    return;
  }

  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  if (!snackbarHandler) {
    return;
  }

  SnackbarMessage* snackbarMessage = [[SnackbarMessage alloc]
      initWithTitle:base::SysUTF8ToNSString(
                        taskInfo->GetCompletionSnackbarMessage())];

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SNACKBAR_ACTION);

  __weak LevelUpSceneAgent* weakSelf = self;
  action.handler = ^{
    LevelUpSceneAgent* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    Browser* strongBrowser = strongSelf.sceneState.browserProviderInterface
                                 .currentBrowserProvider.browser;
    if (!strongBrowser) {
      return;
    }
    id<LevelUpCommands> levelUpHandler = HandlerForProtocol(
        strongBrowser->GetCommandDispatcher(), LevelUpCommands);
    [levelUpHandler showLevelUp];
  };

  snackbarMessage.action = action;

  [snackbarHandler showSnackbarMessage:snackbarMessage];
}

@end
