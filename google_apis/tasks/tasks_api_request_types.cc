// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_request_types.h"

#include <string>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "google_apis/tasks/tasks_api_task_status.h"

namespace google_apis::tasks {
namespace {

constexpr char kApiRequestBodyTaskStatusKey[] = "status";
constexpr char kApiRequestBodyTaskTitleKey[] = "title";

}  // namespace

std::string TaskRequestPayload::ToJson() const {
  base::Value::Dict root;

  if (!title.empty()) {
    root.Set(kApiRequestBodyTaskTitleKey, title);
  }

  if (status != TaskStatus::kUnknown) {
    root.Set(kApiRequestBodyTaskStatusKey, TaskStatusToString(status));
  }

  const auto json = base::WriteJson(root);
  CHECK(json);
  return json.value();
}

}  // namespace google_apis::tasks
