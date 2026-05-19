// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_

#include <map>
#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/level_up/model/task_info.h"
#include "ios/chrome/browser/level_up/model/task_types.h"

// Service that manages the "Level Up" feature, tracking user progress and
// stats. It also holds the definitions of all tasks.
class LevelUpService : public KeyedService {
 public:
  LevelUpService();
  ~LevelUpService() override;

  // Returns true if the user is opted in.
  bool IsOptedIn() const;

  // Opts the user in or out.
  void SetOptIn(bool opted_in);

  // Returns the current level of the user.
  int GetCurrentLevel() const;

  // Returns the TaskInfo for the given TaskType, or nullptr if not found.
  const TaskInfo* GetTaskInfo(TaskType task_type) const;

  // Returns all available tasks.
  const std::map<TaskType, TaskInfo>& GetTasks() const;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  void PopulateTasks();

  std::map<TaskType, TaskInfo> tasks_;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
