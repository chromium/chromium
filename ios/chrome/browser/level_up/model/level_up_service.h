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
#include "ios/chrome/browser/level_up/model/task_info.h"
#include "ios/chrome/browser/level_up/model/task_types.h"

class PrefService;

// Service that manages the "Level Up" feature, tracking user progress and
// stats. It also holds the definitions of all tasks.
class LevelUpService : public KeyedService {
 public:
  LevelUpService(PrefService* pref_service);
  ~LevelUpService() override;

  // Returns true if the user has enabled the feature UI.
  bool IsUIEnabled() const;

  // Enables or disables the feature UI.
  void SetUIEnabled(bool ui_enabled);

  // Returns the current level of the user.
  int GetCurrentLevel() const;

  // Marks a task as completed.
  void MarkTaskCompleted(TaskType task_type);

  // Returns true if the task is completed.
  bool IsTaskCompleted(TaskType task_type) const;

  // Returns the TaskInfo for the given TaskType, or nullptr if not found.
  const TaskInfo* GetTaskInfo(TaskType task_type) const;

  // Returns all available tasks.
  const std::map<TaskType, TaskInfo>& GetTasks() const;

  // KeyedService implementation.
  void Shutdown() override;

  // Registers profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void PopulateTasks();
  void LoadPrefs();

  raw_ptr<PrefService> pref_service_;
  std::map<TaskType, TaskInfo> tasks_;
  // Set of completed task identifiers. Stored as strings rather than TaskType
  // enums to support storing unknown tasks received via sync from newer
  // versions of the app.
  std::set<std::string> completed_tasks_;
  int current_level_ = 1;
  bool is_ui_enabled_ = false;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
