// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "ios/chrome/browser/level_up/model/task_info.h"
#include "ios/chrome/browser/level_up/model/task_types.h"

class BrowserList;
class IOSChromePasswordCheckManager;
class PrefService;
class SessionRestorationService;

// Service that manages the "Level Up" feature, tracking user progress and
// stats. It also holds the definitions of all tasks.
class LevelUpService : public KeyedService {
 public:
  LevelUpService(
      PrefService* pref_service,
      BrowserList* browser_list = nullptr,
      SessionRestorationService* session_restoration_service = nullptr,
      IOSChromePasswordCheckManager* password_check_manager = nullptr);
  ~LevelUpService() override;

  // Returns true if the user is opted in to Level Up.
  bool IsOptedIn() const;

  // Returns true if the user has enabled the feature UI.
  bool IsUIEnabled() const;

  // Enables or disables the feature UI.
  void SetUIEnabled(bool ui_enabled);

  // Returns the current level of the user.
  int GetCurrentLevel() const;

  // Returns the number of tasks remaining to reach the next level, or 0 if
  // the max level is reached.
  int GetTasksRemainingForNextLevel() const;

  // Marks a task as completed.
  void MarkTaskCompleted(TaskType task_type);

  // Resets all task completion status and stats.
  void ResetAllTasksStatus();

  // Returns true if the task is completed.
  bool IsTaskCompleted(TaskType task_type) const;

  // Returns the TaskInfo for the given TaskType, or nullptr if not found.
  const TaskInfo* GetTaskInfo(TaskType task_type) const;

  // Returns the current count/value for the given stat type.
  int GetStatValue(LevelUpTaskStatType stat_type) const;

  // Increments the stat type by `delta`.
  void IncrementStatValue(LevelUpTaskStatType stat_type, int delta = 1);

  // Returns all available tasks.
  const std::map<TaskType, std::unique_ptr<TaskInfo>>& GetTasks() const;

  // Returns a map of user action strings to stat types that trigger stat
  // increments.
  const std::map<std::string, LevelUpTaskStatType>& GetStatTriggerUserActions()
      const;

  // KeyedService implementation.
  void Shutdown() override;

  // Registers profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  class LevelUpPasswordCheckObserver;
  class LevelUpTabGroupObserver;

  // Populates the map of available tasks.
  void PopulateTasks();

  // Loads saved completed tasks and UI state from preferences.
  void LoadPrefs();

  // Recalculates the user's level based on completed tasks, enforcing
  // monotonicity, and updates the saved highest level preference if the level
  // increases.
  void UpdateLevelAndPref();

  // Returns the additional completed tasks required to reach the given level
  // from the previous level.
  int GetTasksIncrementForLevel(int level) const;

  // Returns the total number of completed tasks required to reach the given
  // level.
  int GetTotalTasksRequiredForLevel(int level) const;

  // Calculates the user level based on the count of completed tasks.
  // Declared const since it does not modify any service state.
  int CalculateLevel(size_t completed_count) const;

  // Called when the `kLevelUpUIEnabled` preference changes.
  void OnUIEnabledPrefChanged();

  raw_ptr<PrefService> pref_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<LevelUpPasswordCheckObserver> password_check_observer_;
  std::unique_ptr<LevelUpTabGroupObserver> tab_group_observer_;
  std::map<TaskType, std::unique_ptr<TaskInfo>> tasks_;
  std::map<std::string, LevelUpTaskStatType> stat_trigger_user_actions_;
  // Set of completed task identifiers. Stored as strings rather than TaskType
  // enums to support storing unknown tasks received via sync from newer
  // versions of the app.
  std::set<std::string> completed_tasks_;
  int current_level_ = 1;
  bool is_ui_enabled_ = false;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
