// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_

#include <string>

#include "base/functional/callback.h"
#include "ios/chrome/browser/level_up/model/task_types.h"

// Struct that stores information about a task in the Level Up feature.
struct TaskInfo {
  TaskInfo(TaskType task_type,
           int title_id,
           int task_description_id,
           const std::string& icon_symbol_name,
           bool is_custom_symbol,
           LevelUpTaskCategory category,
           const std::string& trigger_action,
           base::RepeatingClosure navigation_action);
  ~TaskInfo();

  TaskInfo(const TaskInfo& other);
  TaskInfo& operator=(const TaskInfo& other);

  // The unique identifier for the task.
  TaskType task_type;

  // Resource ID for the localized title of the task.
  int title_id;

  // Resource ID for the localized description of the task.
  int task_description_id;

  // Name of the icon asset associated with the task.
  std::string icon_symbol_name;

  // Whether the icon_symbol_name is a custom asset in the bundle.
  bool is_custom_symbol;

  // The category this task belongs to (e.g., for grouping).
  LevelUpTaskCategory category;

  // The user action string that triggers completion of this task.
  std::string trigger_action;

  // Callback to navigate the user to the task's entry point.
  base::RepeatingClosure navigation_action;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_TASK_INFO_H_
