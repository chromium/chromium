// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class PasswordCheckupTaskInfo : public TaskInfo {
 public:
  PasswordCheckupTaskInfo() = default;
  ~PasswordCheckupTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kPasswordCheckup; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_PASSWORD_CHECKUP);
  }
  std::string GetTaskDescription() const override {
    return "Check your saved passwords for security issues";
  }
  Symbol GetIconSymbol() const override { return SymbolPasswordManager; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSafety;
  }
  std::string GetTriggerUserAction() const override {
    return "MobilePasswordCheckupSettingsClose";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_PASSWORD_CHECKUP);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<PopupMenuCommands> handler =
          HandlerForProtocol(dispatcher, PopupMenuCommands);
      [handler showLevelUpPasswordCheckupWalkthroughIPH];
    });
  }
};

std::unique_ptr<TaskInfo> CreatePasswordCheckupTaskInfo() {
  return std::make_unique<PasswordCheckupTaskInfo>();
}
