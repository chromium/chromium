// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class LensWebsiteSearchTaskInfo : public TaskInfo {
 public:
  LensWebsiteSearchTaskInfo() = default;
  ~LensWebsiteSearchTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kLensWebsiteSearch; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_GOOGLE_LENS);
  }
  std::string GetTaskDescription() const override {
    return "Draw, highlight, or tap to search and get results without leaving "
           "your tab";
  }
  Symbol GetIconSymbol() const override { return SymbolCameraLens; }
  bool IsMulticolorIcon() const override { return true; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSearch;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_LENS_WEBSITE_SEARCH);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateLensWebsiteSearchTaskInfo() {
  return std::make_unique<LensWebsiteSearchTaskInfo>();
}
