// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class IncognitoTaskInfo : public TaskInfo {
 public:
  IncognitoTaskInfo() = default;
  ~IncognitoTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kIncognito; }
  std::string GetTitle() const override { return "Go Incognito"; }
  std::string GetTaskDescription() const override {
    return "Open incognito tabs to browse the web privately";
  }
  std::string GetIconSymbolName() const override {
    return base::SysNSStringToUTF8(kIncognitoSymbol);
  }
  bool IsCustomSymbol() const override { return true; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSafety;
  }
  std::string GetTriggerUserAction() const override {
    return "IncognitoMode_Started";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_INCOGNITO);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<SceneCommands> handler = HandlerForProtocol(dispatcher, SceneCommands);
      // TODO(crbug.com/532639936): Also trigger the IPH to swipe right to open
      // incognito (kIPHiOSTabGridSwipeRightForIncognito) once wired up.
      [handler displayTabGridInMode:TabGridOpeningMode::kRegular];
    });
  }
};

std::unique_ptr<TaskInfo> CreateIncognitoTaskInfo() {
  return std::make_unique<IncognitoTaskInfo>();
}
