// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class SafeBrowsingTaskInfo : public TaskInfo {
 public:
  SafeBrowsingTaskInfo() = default;
  ~SafeBrowsingTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kSafeBrowsing; }
  std::string GetTitle() const override { return "Enhanced Safe Browsing"; }
  std::string GetTaskDescription() const override {
    return "Add an extra layer of protection against online threats";
  }
  Symbol GetIconSymbol() const override { return SymbolShield; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSafety;
  }
  std::string GetTriggerUserAction() const override {
    return "MobilePrivacySafeBrowsingSettingsClose";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_SAFE_BROWSING);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<BrowserCoordinatorCommands> handler =
          HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
      [handler showEnhancedSafeBrowsingPromo];
    });
  }
};

std::unique_ptr<TaskInfo> CreateSafeBrowsingTaskInfo() {
  return std::make_unique<SafeBrowsingTaskInfo>();
}
