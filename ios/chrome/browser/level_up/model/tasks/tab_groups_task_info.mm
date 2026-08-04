// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class TabGroupsTaskInfo : public TaskInfo {
 public:
  TabGroupsTaskInfo() = default;
  ~TabGroupsTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kTabGroups; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_TAB_GROUPS);
  }
  std::string GetTaskDescription() const override {
    return "Stay organized with tab groups";
  }
  Symbol GetIconSymbol() const override { return SymbolTabs; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override {
    return "MobileTabGroupUserCreatedNewGroup";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_TAB_GROUPS);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<SceneCommands> sceneHandler =
          HandlerForProtocol(dispatcher, SceneCommands);
      [sceneHandler displayTabGridInMode:TabGridOpeningMode::kRegular];

      id<TabGridCommands> tabGridHandler =
          HandlerForProtocol(dispatcher, TabGridCommands);
      [tabGridHandler presentCreateTabGroupBubble];
    });
  }
};

std::unique_ptr<TaskInfo> CreateTabGroupsTaskInfo() {
  return std::make_unique<TabGroupsTaskInfo>();
}
