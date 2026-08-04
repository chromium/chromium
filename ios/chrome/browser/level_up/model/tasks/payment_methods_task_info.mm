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

class PaymentMethodsTaskInfo : public TaskInfo {
 public:
  PaymentMethodsTaskInfo() = default;
  ~PaymentMethodsTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kPaymentMethods; }
  std::string GetTitle() const override { return "Manage payment methods"; }
  std::string GetTaskDescription() const override {
    return "Add new payment methods or edit saved ones to check out faster";
  }
  Symbol GetIconSymbol() const override { return SymbolCreditCard; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override {
    return "MobileCreditCardSettingsCompleted";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_PAYMENT_METHODS);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher) {
      id<PopupMenuCommands> handler =
          HandlerForProtocol(dispatcher, PopupMenuCommands);
      [handler showLevelUpPaymentMethodsWalkthroughIPH];
    });
  }
};

std::unique_ptr<TaskInfo> CreatePaymentMethodsTaskInfo() {
  return std::make_unique<PaymentMethodsTaskInfo>();
}
