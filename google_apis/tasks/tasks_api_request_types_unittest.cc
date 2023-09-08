// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_request_types.h"

#include "google_apis/tasks/tasks_api_task_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::tasks {

TEST(TasksApiRequestTypesTest, ConvertsToJson) {
  TaskRequestPayload payload = {.status = TaskStatus::kCompleted};
  EXPECT_EQ(payload.ToJson(), "{\"status\":\"completed\"}");
}

}  // namespace google_apis::tasks
