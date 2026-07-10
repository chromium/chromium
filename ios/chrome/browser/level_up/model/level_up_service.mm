// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import <algorithm>
#import <numeric>

#import "base/logging.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// The number of tasks required to move from level `index - 1` to level
// `index`. Indices 0 and 1 are 0 because all users start at level 1.
// The last level requires all remaining tasks to be completed, and so does not
// appear here.
constexpr std::array tasks_per_level = {0, 0, 3, 5};

// The maximum level, dynamically derived from tasks_per_level.
constexpr int kMaxLevel = tasks_per_level.size();

}  // namespace

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

int LevelUpService::GetTasksRemainingForNextLevel() const {
  if (current_level_ >= kMaxLevel) {
    return 0;
  }
  int next_level = current_level_ + 1;
  int required = GetTotalTasksRequiredForLevel(next_level);
  int completed = completed_tasks_.size();
  return std::max(0, required - completed);
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

    UpdateLevelAndPref();
    pref_service_->SetInteger(
        prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness, 0);
  }
}

bool LevelUpService::IsTaskCompleted(TaskType task_type) const {
  std::string storage_id = TaskTypeToString(task_type);
  return completed_tasks_.contains(storage_id);
}

const TaskInfo* LevelUpService::GetTaskInfo(TaskType task_type) const {
  auto it = tasks_.find(task_type);
  if (it != tasks_.end()) {
    return it->second.get();
  }
  return nullptr;
}

const std::map<TaskType, std::unique_ptr<TaskInfo>>& LevelUpService::GetTasks()
    const {
  return tasks_;
}

void LevelUpService::PopulateTasks() {
  tasks_[TaskType::kTabGroups] = CreateTabGroupsTaskInfo();
  tasks_[TaskType::kAutofill] = CreateAutofillTaskInfo();
  tasks_[TaskType::kPinTabs] = CreatePinTabsTaskInfo();
  tasks_[TaskType::kGemini] = CreateGeminiTaskInfo();
  tasks_[TaskType::kPaymentMethods] = CreatePaymentMethodsTaskInfo();
  tasks_[TaskType::kQuickDelete] = CreateQuickDeleteTaskInfo();
  tasks_[TaskType::kSafeBrowsing] = CreateSafeBrowsingTaskInfo();
  tasks_[TaskType::kIncognito] = CreateIncognitoTaskInfo();
  tasks_[TaskType::kPasswordCheckup] = CreatePasswordCheckupTaskInfo();
  tasks_[TaskType::kLensSearch] = CreateLensSearchTaskInfo();
  tasks_[TaskType::kAISearch] = CreateAISearchTaskInfo();
  tasks_[TaskType::kCameraSearch] = CreateCameraSearchTaskInfo();
}

void LevelUpService::LoadPrefs() {
  is_ui_enabled_ = pref_service_->GetBoolean(prefs::kLevelUpUIEnabled);

  const base::ListValue& list =
      pref_service_->GetList(prefs::kLevelUpCompletedTasks);
  for (const auto& value : list) {
    if (value.is_string()) {
      completed_tasks_.insert(value.GetString());
    }
  }

  UpdateLevelAndPref();
}

void LevelUpService::UpdateLevelAndPref() {
  int highest_level_pref =
      pref_service_->GetInteger(prefs::kLevelUpHighestLevel);
  int calculated_level = CalculateLevel(completed_tasks_.size());
  current_level_ =
      std::max({current_level_, highest_level_pref, calculated_level});

  if (current_level_ > highest_level_pref) {
    pref_service_->SetInteger(prefs::kLevelUpHighestLevel, current_level_);
  }
}

int LevelUpService::GetTasksIncrementForLevel(int level) const {
  if (level < 1 || level > kMaxLevel) {
    return 0;
  }

  if (level == kMaxLevel) {
    int tasks_left = tasks_.size() - std::accumulate(tasks_per_level.begin(),
                                                     tasks_per_level.end(), 0);
    return std::max(0, tasks_left);
  }
  return tasks_per_level[level];
}

int LevelUpService::GetTotalTasksRequiredForLevel(int level) const {
  level = std::clamp(level, 1, kMaxLevel);
  if (level == kMaxLevel) {
    return tasks_.size();
  }
  return std::accumulate(tasks_per_level.begin(),
                         tasks_per_level.begin() + level + 1, 0);
}

int LevelUpService::CalculateLevel(size_t completed_count) const {
  int running_task_sum = 0;
  for (int level = 2; level <= kMaxLevel; ++level) {
    running_task_sum += GetTasksIncrementForLevel(level);
    // If the completed count is less than the running sum to reach a level,
    // the user's active level is the previous level.
    if (completed_count < static_cast<size_t>(running_task_sum)) {
      return level - 1;
    }
  }
  return kMaxLevel;
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
      prefs::kLevelUpHighestLevel, 1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLevelUpUIEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
