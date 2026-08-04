// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class CameraSearchTaskInfo : public TaskInfo {
 public:
  CameraSearchTaskInfo() = default;
  ~CameraSearchTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kCameraSearch; }
  std::string GetTitle() const override { return "Search with camera"; }
  std::string GetTaskDescription() const override {
    return "Shop, translate and identify what you see with your camera";
  }
  Symbol GetIconSymbol() const override { return SymbolCamera; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSearch;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_CAMERA_SEARCH);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateCameraSearchTaskInfo() {
  return std::make_unique<CameraSearchTaskInfo>();
}
