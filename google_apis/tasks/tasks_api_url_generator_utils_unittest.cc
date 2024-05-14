// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_url_generator_utils.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::tasks {

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTaskListsUrl) {
  EXPECT_EQ(GetListTaskListsUrl(/*max_results=*/std::nullopt,
                                /*page_token=*/""),
            "https://tasks.googleapis.com/tasks/v1/users/@me/lists"
            "?fields=kind%2Citems(id%2Ctitle%2Cupdated)%2CnextPageToken");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTaskListsUrlWithOptionalArgs) {
  EXPECT_EQ(GetListTaskListsUrl(/*max_results=*/100,
                                /*page_token=*/"qwerty"),
            "https://tasks.googleapis.com/tasks/v1/users/@me/lists"
            "?fields=kind%2Citems(id%2Ctitle%2Cupdated)%2CnextPageToken"
            "&maxResults=100"
            "&pageToken=qwerty");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTasksUrl) {
  EXPECT_EQ(
      GetListTasksUrl("task-list-id", /*include_completed=*/false,
                      /*include_assigned=*/false,
                      /*max_results=*/std::nullopt,
                      /*page_token=*/""),
      "https://tasks.googleapis.com/tasks/v1/lists/task-list-id/tasks"
      "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent%2Cposition"
      "%2Cdue%2Clinks(type)%2Cnotes%2Cupdated%2CwebViewLink)%2CnextPageToken"
      "&showCompleted=false");

  EXPECT_EQ(GetListTasksUrl("task-list-id", /*include_completed=*/false,
                            /*include_assigned=*/true,
                            /*max_results=*/std::nullopt,
                            /*page_token=*/""),
            "https://tasks.googleapis.com/tasks/v1/lists/task-list-id/tasks"
            "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent%2Cposition"
            "%2Cdue%2Clinks(type)%2Cnotes%2Cupdated%2CwebViewLink"
            "%2CassignmentInfo(surfaceType))%2CnextPageToken"
            "&showCompleted=false"
            "&showAssigned=true");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsListTasksUrlWithOptionalArgs) {
  EXPECT_EQ(
      GetListTasksUrl("task-list-id", /*include_completed=*/true,
                      /*include_assigned=*/false,
                      /*max_results=*/100,
                      /*page_token=*/"qwerty"),
      "https://tasks.googleapis.com/tasks/v1/lists/task-list-id/tasks"
      "?fields=kind%2Citems(id%2Ctitle%2Cstatus%2Cparent%2Cposition%"
      "2Cdue%2Clinks(type)%2Cnotes%2Cupdated%2CwebViewLink)%2CnextPageToken"
      "&maxResults=100"
      "&pageToken=qwerty");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsPatchTaskUrl) {
  EXPECT_EQ(GetPatchTaskUrl("task-list-id", "task-id"),
            "https://tasks.googleapis.com/tasks/v1/"
            "lists/task-list-id/tasks/task-id");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsInsertTaskUrl) {
  EXPECT_EQ(GetInsertTaskUrl("task-list-id", /*previous_task_id=*/""),
            "https://tasks.googleapis.com/tasks/v1/lists/task-list-id/tasks");
}

TEST(TasksApiUrlGeneratorUtilsTest, ReturnsInsertTaskUrlWithOptionalArgs) {
  EXPECT_EQ(GetInsertTaskUrl("task-list-id", "previous-task-id"),
            "https://tasks.googleapis.com/tasks/v1/lists/task-list-id/tasks"
            "?previous=previous-task-id");
}

}  // namespace google_apis::tasks
