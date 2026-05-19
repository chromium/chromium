// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "components/prefs/pref_service.h"

LevelUpService::LevelUpService() {
  PopulateTasks();
}

LevelUpService::~LevelUpService() = default;

bool LevelUpService::IsOptedIn() const {
  // TODO(crbug.com/513246860): Implement.
  return false;
}

void LevelUpService::SetOptIn(bool opted_in) {
  // TODO(crbug.com/513246860): Implement.
}

int LevelUpService::GetCurrentLevel() const {
  // TODO(crbug.com/513246860): Implement.
  return 0;
}

const TaskInfo* LevelUpService::GetTaskInfo(TaskType task_type) const {
  auto it = tasks_.find(task_type);
  if (it != tasks_.end()) {
    return &it->second;
  }
  return nullptr;
}

const std::map<TaskType, TaskInfo>& LevelUpService::GetTasks() const {
  return tasks_;
}

void LevelUpService::PopulateTasks() {
  // Add Tab Groups task as a sample.
  tasks_.insert(std::make_pair(
      TaskType::kTabGroups,
      TaskInfo(TaskType::kTabGroups, /*title_id=*/0, /*task_description_id=*/0,
               /*icon_symbol_name=*/"tab_groups_icon",
               /*category=*/"Tabs",
               /*trigger_action=*/"TabGroupCreated", base::BindRepeating([]() {
                 // TODO(crbug.com/513245990): Implement navigation.
               }))));
}

void LevelUpService::Shutdown() {
  // TODO(crbug.com/513246860): Implement if needed.
}
