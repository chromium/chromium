// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_url_generator_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace google_apis::tasks {

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTaskListsUrl) {
  EXPECT_EQ(GetListTaskListsUrl(/*max_results=*/absl::nullopt,
                                /*page_token=*/"")
                .spec(),
            "https://www.googleapis.com/tasks/v1/users/@me/lists"
            "?fields=kind%2Citems(id%2Ctitle%2Cupdated)");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTaskListsUrlWithOptionalArgs) {
  EXPECT_EQ(GetListTaskListsUrl(/*max_results=*/100,
                                /*page_token=*/"qwerty")
                .spec(),
            "https://www.googleapis.com/tasks/v1/users/@me/lists"
            "?fields=kind%2Citems(id%2Ctitle%2Cupdated)"
            "&maxResults=100"
            "&pageToken=qwerty");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTasksUrl) {
  EXPECT_EQ(GetListTasksUrl("task-id", /*include_completed=*/false,
                            /*max_results=*/absl::nullopt,
                            /*page_token=*/"")
                .spec(),
            "https://www.googleapis.com/tasks/v1/lists/task-id/tasks"
            "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent)"
            "&showCompleted=false");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTasksUrlWithOptionalArgs) {
  EXPECT_EQ(GetListTasksUrl("task-id", /*include_completed=*/true,
                            /*max_results=*/100,
                            /*page_token=*/"qwerty")
                .spec(),
            "https://www.googleapis.com/tasks/v1/lists/task-id/tasks"
            "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent)"
            "&showCompleted=true"
            "&maxResults=100"
            "&pageToken=qwerty");
}

}  // namespace google_apis::tasks
