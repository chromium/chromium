// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class GeminiTaskInfo : public TaskInfo {
 public:
  GeminiTaskInfo() = default;
  ~GeminiTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kGemini; }
  std::string GetTitle() const override { return "Use Gemini in Chrome"; }
  std::string GetTaskDescription() const override {
    return "Get answers faster with Gemini in Chrome";
  }
  Symbol GetIconSymbol() const override {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    return SymbolGeminiBrandedLogo;
#else
    return SymbolGeminiNonBrandedLogo;
#endif
  }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_GEMINI);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateGeminiTaskInfo() {
  return std::make_unique<GeminiTaskInfo>();
}
