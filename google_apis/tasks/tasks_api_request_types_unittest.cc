// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_request_types.h"

#include <string>

#include "google_apis/tasks/tasks_api_task_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::tasks {

TEST(TasksApiRequestTypesTest, ConvertsToJson) {
  struct {
    TaskRequestPayload payload;
    std::string expected_json;
  } test_cases[] = {
      {{.status = TaskStatus::kCompleted}, "{\"status\":\"completed\"}"},
      {{.status = TaskStatus::kNeedsAction}, "{\"status\":\"needsAction\"}"},
      {{.status = TaskStatus::kUnknown}, "{}"},
      {{.title = "Lorem ipsum dolor sit amet",
        .status = TaskStatus::kCompleted},
       "{\"status\":\"completed\",\"title\":\"Lorem ipsum dolor sit amet\"}"},
      {{.title = "Lorem ipsum dolor sit amet",
        .status = TaskStatus::kNeedsAction},
       "{\"status\":\"needsAction\",\"title\":\"Lorem ipsum dolor sit amet\"}"},
      {{.title = "Lorem ipsum dolor sit amet", .status = TaskStatus::kUnknown},
       "{\"title\":\"Lorem ipsum dolor sit amet\"}"},
  };

  for (const auto& tc : test_cases) {
    EXPECT_EQ(tc.payload.ToJson(), tc.expected_json);
  }
}

}  // namespace google_apis::tasks
