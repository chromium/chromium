// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "ios/chrome/browser/level_up/model/task_info.h"
#include "ios/chrome/browser/level_up/model/task_types.h"

class BrowserList;
class IOSChromePasswordCheckManager;
class PrefService;
class SessionRestorationService;
class LevelUpServiceObserver;

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

  // Returns the user's current level, which is the higher of the stored
  // highest-level pref and the level calculated from completed task that are
  // available on this device.
  int GetCurrentLevel() const;

  // Returns a pair of (the number of tasks remaining to reach the next level,
  // total number of tasks required for the next level), or (0,0) if the max
  // level is reached.
  std::pair<int, int> GetTasksRemainingForNextLevel() const;

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

  // Returns all stat types ordered by the completion recency of their
  // associated task (most recent first).
  std::vector<LevelUpTaskStatType> GetOrderedStatTypes() const;

  // Increments the stat type by `delta`.
  void IncrementStatValue(LevelUpTaskStatType stat_type, int delta = 1);

  // Returns up to 4 recommended tasks prioritized by category recency.
  std::vector<const TaskInfo*> GetRecommendedTasks() const;

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

  // Adds/Removes LevelUpServiceObserver observers.
  void AddObserver(LevelUpServiceObserver* observer);
  void RemoveObserver(LevelUpServiceObserver* observer);

 private:
  class LevelUpPasswordCheckObserver;
  class LevelUpTabGroupObserver;

  // Notifies observers of the level up level and pref changes.
  void NotifyObserversOfTaskComplete();

  // Notifies observers of stat value changes.
  void NotifyObserversOfStatUpdate();

  // Populates the map of available tasks.
  void PopulateTasks();

  // Returns the list of completed task as strings from preferences in the
  // order they were completed.
  std::vector<std::string> GetCompletedTasks() const;

  // Updates the kLevelUpHighestLevel when level has changed locally.
  void UpdateHighestLevelPref();

  // Returns the additional completed tasks required to reach the given level
  // from the previous level.
  int GetTasksIncrementForLevel(int level) const;

  // Returns the total number of completed tasks required to reach the given
  // level.
  int GetTotalTasksRequiredForLevel(int level) const;

  // Calculates the user level based on the count of completed tasks.
  // Declared const since it does not modify any service state.
  int CalculateLevel(size_t completed_count) const;

  // Returns the number of completed tasks that are available on this device.
  size_t GetAvailableCompletedTaskCount() const;

  // Called when the `kLevelUpOptIn` preference changes.
  void OnOptInPrefChanged();

  // Called when `kLevelUpCompletedTasks` or `kLevelUpHighestLevel` preferences
  // changes.
  void OnCompletedTasksAndHighestLevelPrefsChanged();

  // Called when any Level Up stat preference changes.
  void OnStatsPrefsChanged();

  raw_ptr<PrefService> pref_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<LevelUpPasswordCheckObserver> password_check_observer_;
  std::unique_ptr<LevelUpTabGroupObserver> tab_group_observer_;
  std::map<TaskType, std::unique_ptr<TaskInfo>> tasks_;
  std::map<std::string, LevelUpTaskStatType> stat_trigger_user_actions_;
  base::ObserverList<LevelUpServiceObserver> observers_;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
