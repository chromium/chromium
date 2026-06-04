// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/task_info.h"

TaskInfo::TaskInfo(TaskType task_type,
                   int title_id,
                   int task_description_id,
                   const std::string& icon_symbol_name,
                   bool is_custom_symbol,
                   LevelUpTaskCategory category,
                   const std::string& trigger_action,
                   base::RepeatingClosure navigation_action)
    : task_type(task_type),
      title_id(title_id),
      task_description_id(task_description_id),
      icon_symbol_name(icon_symbol_name),
      is_custom_symbol(is_custom_symbol),
      category(category),
      trigger_action(trigger_action),
      navigation_action(std::move(navigation_action)) {}

TaskInfo::~TaskInfo() = default;

TaskInfo::TaskInfo(const TaskInfo& other) = default;
TaskInfo& TaskInfo::operator=(const TaskInfo& other) = default;
