// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tips_passwords_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class AutofillTaskInfo : public TaskInfo {
 public:
  AutofillTaskInfo() = default;
  ~AutofillTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kAutofill; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_FEATURE_PASSWORDS_AUTOFILL);
  }
  std::string GetTaskDescription() const override {
    return "Quickly sign into sites and apps with your saved passwords";
  }
  Symbol GetIconSymbol() const override { return SymbolPasswordManager; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_AUTOFILL);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<TipsPasswordsCommands> handler =
          HandlerForProtocol(dispatcher, TipsPasswordsCommands);
      [handler showPasswordsTipForIdentifier:segmentation_platform::
                                                 TipIdentifier::kSavePasswords];
    });
  }
};

std::unique_ptr<TaskInfo> CreateAutofillTaskInfo() {
  return std::make_unique<AutofillTaskInfo>();
}
