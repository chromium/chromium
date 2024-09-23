// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_task_status.h"

#include <string>
#include <string_view>

#include "base/notreached.h"

namespace google_apis::tasks {
namespace {

constexpr char kTaskStatusCompleted[] = "completed";
constexpr char kTaskStatusNeedsAction[] = "needsAction";

}  // namespace

TaskStatus TaskStatusFromString(std::string_view task_status) {
  if (task_status == kTaskStatusCompleted) {
    return TaskStatus::kCompleted;
  }
  if (task_status == kTaskStatusNeedsAction) {
    return TaskStatus::kNeedsAction;
  }
  return TaskStatus::kUnknown;
}

std::string TaskStatusToString(TaskStatus task_status) {
  switch (task_status) {
    case TaskStatus::kCompleted:
      return kTaskStatusCompleted;
    case TaskStatus::kNeedsAction:
      return kTaskStatusNeedsAction;
    case TaskStatus::kUnknown:
      NOTREACHED();
  }
}

}  // namespace google_apis::tasks
