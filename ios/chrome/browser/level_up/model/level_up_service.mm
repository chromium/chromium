// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import <algorithm>
#import <numeric>

#import "base/logging.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// The number of tasks required to move from level `index - 1` to level
// `index`. Indices 0 and 1 are 0 because all users start at level 1.
// The last level requires all remaining tasks to be completed, and so does not
// appear here.
constexpr std::array tasks_per_level = {0, 0, 3, 5};

// The maximum level, dynamically derived from tasks_per_level.
constexpr int kMaxLevel = tasks_per_level.size();

const char* GetPrefNameForStatType(LevelUpTaskStatType stat_type) {
  switch (stat_type) {
    case LevelUpTaskStatType::kTabsDecluttered:
      return prefs::kLevelUpTabsDeclutteredStat;
    case LevelUpTaskStatType::kTypingSaved:
      return prefs::kLevelUpTypingSavedStat;
    case LevelUpTaskStatType::kPasswordsVerified:
      return prefs::kLevelUpPasswordsVerifiedStat;
    case LevelUpTaskStatType::kPhotoSearchesPerformed:
      return prefs::kLevelUpPhotoSearchesPerformedStat;
  }
}

}  // namespace

// Helper observer class that monitors tab group operations (creation,
// additions, and moves) across all active browsers in the profile to
// dynamically track and increment the Level Up `kTabsDecluttered` stat. Listens
// to SessionRestoration events to ignore startup session restoration
// operations.
class LevelUpService::LevelUpTabGroupObserver
    : public BrowserListObserver,
      public WebStateListObserver,
      public SessionRestorationObserver {
 public:
  LevelUpTabGroupObserver(
      LevelUpService* service,
      BrowserList* browser_list,
      SessionRestorationService* session_restoration_service)
      : service_(service) {
    if (browser_list) {
      browser_list_observation_.Observe(browser_list);
      for (Browser* browser : browser_list->BrowsersOfType(
               BrowserList::BrowserType::kRegularAndInactive)) {
        web_state_list_observation_.AddObservation(browser->GetWebStateList());
      }
    }
    if (session_restoration_service) {
      session_restoration_observation_.Observe(session_restoration_service);
    }
  }

  ~LevelUpTabGroupObserver() override = default;

  void Shutdown() {
    browser_list_observation_.Reset();
    web_state_list_observation_.RemoveAllObservations();
    session_restoration_observation_.Reset();
  }

  // BrowserListObserver implementation.
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override {
    if (browser->type() != Browser::Type::kRegular &&
        browser->type() != Browser::Type::kInactive) {
      return;
    }
    web_state_list_observation_.AddObservation(browser->GetWebStateList());
  }

  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override {
    if (browser->type() != Browser::Type::kRegular &&
        browser->type() != Browser::Type::kInactive) {
      return;
    }
    web_state_list_observation_.RemoveObservation(browser->GetWebStateList());
  }

  void OnBrowserListShutdown(BrowserList* browser_list) override {
    browser_list_observation_.Reset();
    web_state_list_observation_.RemoveAllObservations();
  }

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    if (is_restoring_session_) {
      return;
    }

    switch (change.type()) {
      case WebStateListChange::Type::kStatusOnly: {
        const auto& status_change = change.As<WebStateListChangeStatusOnly>();
        if (!status_change.old_group() && status_change.new_group()) {
          service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered,
                                       1);
        }
        break;
      }
      case WebStateListChange::Type::kMove: {
        const auto& move_change = change.As<WebStateListChangeMove>();
        if (!move_change.old_group() && move_change.new_group()) {
          service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered,
                                       1);
        }
        break;
      }
      case WebStateListChange::Type::kInsert: {
        const auto& insert_change = change.As<WebStateListChangeInsert>();
        if (insert_change.group()) {
          service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered,
                                       1);
        }
        break;
      }
      default:
        break;
    }
  }

  void WebStateListDestroyed(WebStateList* web_state_list) override {
    web_state_list_observation_.RemoveObservation(web_state_list);
  }

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) override {
    is_restoring_session_ = true;
  }

  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) override {
    is_restoring_session_ = false;
  }

 private:
  raw_ptr<LevelUpService> service_;
  bool is_restoring_session_ = false;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedObservation<SessionRestorationService, SessionRestorationObserver>
      session_restoration_observation_{this};
};

LevelUpService::LevelUpService(
    PrefService* pref_service,
    BrowserList* browser_list,
    SessionRestorationService* session_restoration_service)
    : pref_service_(pref_service) {
  if (!IsLevelUpEnabled()) {
    return;
  }
  PopulateTasks();
  LoadPrefs();

  tab_group_observer_ = std::make_unique<LevelUpTabGroupObserver>(
      this, browser_list, session_restoration_service);
}

LevelUpService::~LevelUpService() = default;

void LevelUpService::Shutdown() {
  if (tab_group_observer_) {
    tab_group_observer_->Shutdown();
  }
}

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

int LevelUpService::GetStatValue(LevelUpTaskStatType stat_type) const {
  const char* pref_name = GetPrefNameForStatType(stat_type);
  return pref_service_->GetInteger(pref_name);
}

void LevelUpService::IncrementStatValue(LevelUpTaskStatType stat_type,
                                        int delta) {
  if (delta <= 0) {
    return;
  }
  const char* pref_name = GetPrefNameForStatType(stat_type);
  int current = GetStatValue(stat_type);
  pref_service_->SetInteger(pref_name, current + delta);
}

const std::map<TaskType, std::unique_ptr<TaskInfo>>& LevelUpService::GetTasks()
    const {
  return tasks_;
}

const std::map<std::string, LevelUpTaskStatType>&
LevelUpService::GetStatTriggerUserActions() const {
  return stat_trigger_user_actions_;
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

  stat_trigger_user_actions_["Mobile.LensOverlay.CameraSearch.Performed"] =
      LevelUpTaskStatType::kPhotoSearchesPerformed;
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
  registry->RegisterIntegerPref(
      prefs::kLevelUpTabsDeclutteredStat, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLevelUpTypingSavedStat, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLevelUpPasswordsVerifiedStat, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLevelUpPhotoSearchesPerformedStat, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
