// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_TASK_STATUS_H_
#define GOOGLE_APIS_TASKS_TASKS_API_TASK_STATUS_H_

#include <string>
#include <string_view>

namespace google_apis::tasks {

// Status of the task.
// For more details see the `status` field at
// https://developers.google.com/tasks/reference/rest/v1/tasks#resource:-task.
enum class TaskStatus {
  kUnknown,
  kNeedsAction,
  kCompleted,
};

// Converts `task_status` string to its enum value equivalent.
TaskStatus TaskStatusFromString(std::string_view task_status);

// Converts `task_status` enum value to string.
std::string TaskStatusToString(TaskStatus task_status);

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_TASK_STATUS_H_
