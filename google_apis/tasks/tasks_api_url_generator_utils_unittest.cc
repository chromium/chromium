// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_url_generator_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::tasks {

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTaskListsUrl) {
  EXPECT_EQ(GetListTaskListsUrl().spec(),
            "https://www.googleapis.com/tasks/v1/users/@me/lists"
            "?fields=kind%2Citems(id%2Ctitle%2Cupdated)");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTasksUrl) {
  EXPECT_EQ(GetListTasksUrl("task-id").spec(),
            "https://www.googleapis.com/tasks/v1/lists/task-id/tasks"
            "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent)");
}

}  // namespace google_apis::tasks
