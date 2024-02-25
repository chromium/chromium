// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_TASKS_TASKS_API_REQUEST_TYPES_H_

#include <string>

#include "google_apis/tasks/tasks_api_task_status.h"

namespace google_apis::tasks {

// Used as a request body for API requests that create or modify a task.
struct TaskRequestPayload {
 public:
  // Converts the struct to JSON format.
  std::string ToJson() const;

  // Title of the task.
  std::string title;

  // Status of the task.
  TaskStatus status = TaskStatus::kUnknown;
};

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_REQUEST_TYPES_H_
