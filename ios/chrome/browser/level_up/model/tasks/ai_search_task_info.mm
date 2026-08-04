// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class AISearchTaskInfo : public TaskInfo {
 public:
  AISearchTaskInfo() = default;
  ~AISearchTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kAISearch; }
  std::string GetTitle() const override { return "Search with AI Mode"; }
  std::string GetTaskDescription() const override {
    return "Ask anything and get the best of the web";
  }
  Symbol GetIconSymbol() const override { return SymbolMagnifyingglassSpark; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSearch;
  }
  std::string GetTriggerUserAction() const override {
    return kNTPMIAEntryPointTappedAction;
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_AI_SEARCH);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<NewTabPageCommands> handler =
          HandlerForProtocol(dispatcher, NewTabPageCommands);
      [handler presentAIModeBubble];
    });
  }
};

std::unique_ptr<TaskInfo> CreateAISearchTaskInfo() {
  return std::make_unique<AISearchTaskInfo>();
}
