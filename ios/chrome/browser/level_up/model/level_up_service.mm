// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

LevelUpService::LevelUpService(PrefService* pref_service)
    : pref_service_(pref_service) {
  if (!IsLevelUpEnabled()) {
    return;
  }
  PopulateTasks();
  LoadPrefs();
}

LevelUpService::~LevelUpService() = default;

bool LevelUpService::IsUIEnabled() const {
  return is_ui_enabled_;
}

void LevelUpService::SetUIEnabled(bool ui_enabled) {
  if (is_ui_enabled_ == ui_enabled) {
    return;
  }
  is_ui_enabled_ = ui_enabled;
  pref_service_->SetBoolean(prefs::kLevelUpUIEnabled, is_ui_enabled_);
}

int LevelUpService::GetCurrentLevel() const {
  return current_level_;
}

void LevelUpService::MarkTaskCompleted(TaskType task_type) {
  std::string storage_id = TaskTypeToString(task_type);
  if (storage_id == TaskTypeToString(TaskType::kUnknown)) {
    return;
  }

  if (completed_tasks_.insert(storage_id).second) {
    // Update prefs.
    ScopedListPrefUpdate update(pref_service_, prefs::kLevelUpCompletedTasks);
    update->Append(storage_id);

    // Recalculate level.
    int new_level = 0;
    bool all_tasks_completed = true;
    for (const auto& [type, info] : tasks_) {
      if (!completed_tasks_.contains(TaskTypeToString(type))) {
        all_tasks_completed = false;
        break;
      }
    }
    if (all_tasks_completed) {
      new_level = 1;
    }

    if (new_level > current_level_) {
      current_level_ = new_level;
      pref_service_->SetInteger(prefs::kLevelUpHighestLevel, current_level_);
    }
  }
}

bool LevelUpService::IsTaskCompleted(TaskType task_type) const {
  std::string storage_id = TaskTypeToString(task_type);
  return completed_tasks_.contains(storage_id);
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

void LevelUpService::LoadPrefs() {
  is_ui_enabled_ = pref_service_->GetBoolean(prefs::kLevelUpUIEnabled);
  current_level_ = pref_service_->GetInteger(prefs::kLevelUpHighestLevel);

  const base::ListValue& list =
      pref_service_->GetList(prefs::kLevelUpCompletedTasks);
  for (const auto& value : list) {
    if (value.is_string()) {
      completed_tasks_.insert(value.GetString());
    }
  }
}

void LevelUpService::Shutdown() {
  // TODO(crbug.com/513246860): Implement if needed.
}

// static
void LevelUpService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kLevelUpCompletedTasks,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLevelUpHighestLevel, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLevelUpUIEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
