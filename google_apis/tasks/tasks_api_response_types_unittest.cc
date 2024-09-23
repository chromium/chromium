// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "google_apis/common/time_util.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::tasks {

using ::base::JSONReader;

TEST(TasksApiResponseTypesTest, CreatesTaskListsFromResponse) {
  const auto raw_task_lists = JSONReader::Read(R"(
      {
        "kind": "tasks#taskLists",
        "items": [
          {
            "kind": "tasks#taskList",
            "id": "qwerty",
            "title": "My Tasks 1",
            "updated": "2023-01-30T22:19:22.812Z"
          },
          {
            "kind": "tasks#taskList",
            "id": "asdfgh",
            "title": "My Tasks 2",
            "updated": "2022-12-21T23:38:22.590Z"
          }
        ]
      })");
  ASSERT_TRUE(raw_task_lists);

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
  ASSERT_TRUE(task_lists);
  EXPECT_TRUE(task_lists->next_page_token().empty());
  EXPECT_EQ(task_lists->items().size(), 2u);

  EXPECT_EQ(task_lists->items()[0]->id(), "qwerty");
  EXPECT_EQ(task_lists->items()[0]->title(), "My Tasks 1");
  EXPECT_EQ(util::FormatTimeAsString(task_lists->items()[0]->updated()),
            "2023-01-30T22:19:22.812Z");

  EXPECT_EQ(task_lists->items()[1]->id(), "asdfgh");
  EXPECT_EQ(task_lists->items()[1]->title(), "My Tasks 2");
  EXPECT_EQ(util::FormatTimeAsString(task_lists->items()[1]->updated()),
            "2022-12-21T23:38:22.590Z");
}

TEST(TasksApiResponseTypesTest, CreatesTaskListsWithNextPageTokenFromResponse) {
  const auto raw_task_lists = JSONReader::Read(R"(
      {
        "kind": "tasks#taskLists",
        "items": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_task_lists);

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
  ASSERT_TRUE(task_lists);
  EXPECT_EQ(task_lists->next_page_token(), "qwerty");
}

TEST(TasksApiResponseTypesTest, FailsToCreateTaskListsFromInvalidResponse) {
  const auto raw_task_lists = JSONReader::Read(R"(
      {
        "kind": "invalid_kind",
        "items": true
      })");
  ASSERT_TRUE(raw_task_lists);

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
  ASSERT_FALSE(task_lists);
}

TEST(TasksApiResponseTypesTest, CreatesTasksFromResponse) {
  const auto raw_tasks = JSONReader::Read(R"(
      {
        "kind": "tasks#tasks",
        "items": [
          {
            "id": "qwe",
            "title": "Completed child task",
            "parent": "asd",
            "position": "00000000000000000000",
            "status": "completed",
            "updated": "2023-01-30T22:19:22.812Z",
            "webViewLink": "https://tasks.google.com/task/id123"
          },
          {
            "id": "asd",
            "title": "Parent task",
            "position": "00000000000000000001",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z",
            "notes": "Lorem ipsum dolor sit amet",
            "updated": "2022-12-21T23:38:22.590Z",
            "webViewLink": "invalid_url"
          }
        ]
      })");
  ASSERT_TRUE(raw_tasks);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  ASSERT_TRUE(tasks);
  EXPECT_TRUE(tasks->next_page_token().empty());
  EXPECT_EQ(tasks->items().size(), 2u);

  EXPECT_EQ(tasks->items()[0]->id(), "qwe");
  EXPECT_EQ(tasks->items()[0]->title(), "Completed child task");
  EXPECT_EQ(tasks->items()[0]->status(), TaskStatus::kCompleted);
  EXPECT_EQ(tasks->items()[0]->parent_id(), "asd");
  EXPECT_EQ(tasks->items()[0]->position(), "00000000000000000000");
  EXPECT_FALSE(tasks->items()[0]->due());
  EXPECT_TRUE(tasks->items()[0]->notes().empty());
  EXPECT_EQ(util::FormatTimeAsString(tasks->items()[0]->updated()),
            "2023-01-30T22:19:22.812Z");
  EXPECT_EQ(tasks->items()[0]->web_view_link(),
            "https://tasks.google.com/task/id123");
  EXPECT_FALSE(tasks->items()[0]->assignment_info());

  EXPECT_EQ(tasks->items()[1]->id(), "asd");
  EXPECT_EQ(tasks->items()[1]->title(), "Parent task");
  EXPECT_EQ(tasks->items()[1]->status(), TaskStatus::kNeedsAction);
  EXPECT_TRUE(tasks->items()[1]->parent_id().empty());
  EXPECT_EQ(tasks->items()[1]->position(), "00000000000000000001");
  EXPECT_EQ(util::FormatTimeAsString(tasks->items()[1]->due().value()),
            "2023-04-19T00:00:00.000Z");
  EXPECT_EQ(tasks->items()[1]->notes(), "Lorem ipsum dolor sit amet");
  EXPECT_EQ(util::FormatTimeAsString(tasks->items()[1]->updated()),
            "2022-12-21T23:38:22.590Z");
  EXPECT_FALSE(tasks->items()[1]->web_view_link().is_valid());
  EXPECT_FALSE(tasks->items()[1]->assignment_info());
}

TEST(TasksApiResponseTypesTest, CreatesTasksWithNextPageTokenFromResponse) {
  const auto raw_tasks = JSONReader::Read(R"(
      {
        "kind": "tasks#tasks",
        "items": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_tasks);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  ASSERT_TRUE(tasks);
  EXPECT_EQ(tasks->next_page_token(), "qwerty");
}

TEST(TasksApiResponseTypesTest, ConvertsTaskLinks) {
  const auto raw_tasks = JSONReader::Read(R"(
      {
        "kind": "tasks#tasks",
        "items": [
          {
            "id": "qwerty",
            "links": [
              {"type": "email"},
              {"type": "something unsupported yet"}
            ]
          }
        ]
      })");
  ASSERT_TRUE(raw_tasks);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  ASSERT_TRUE(tasks);
  ASSERT_EQ(tasks->items().size(), 1u);

  EXPECT_EQ(tasks->items().at(0)->id(), "qwerty");

  const auto& links = tasks->items().at(0)->links();
  ASSERT_EQ(links.size(), 2u);
  EXPECT_EQ(links.at(0)->type(), TaskLink::Type::kEmail);
  EXPECT_EQ(links.at(1)->type(), TaskLink::Type::kUnknown);
}

TEST(TasksApiResponseTypesTest, ConvertsTaskAssignmentInfo) {
  const auto raw_tasks = JSONReader::Read(R"(
      {
        "kind": "tasks#tasks",
        "items": [
          {
            "id": "doc",
            "assignmentInfo": {
              "surfaceType": "DOCUMENT"
            }
          },
          {
            "id": "space",
            "assignmentInfo": {
              "surfaceType": "SPACE"
            }
          },
          {
            "id": "unknown",
            "assignmentInfo": {
              "surfaceType": "UNKNOWN"
            }
          }
        ]
      })");
  ASSERT_TRUE(raw_tasks);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  ASSERT_TRUE(tasks);
  ASSERT_EQ(tasks->items().size(), 3u);

  EXPECT_EQ(tasks->items()[0]->id(), "doc");
  ASSERT_TRUE(tasks->items()[0]->assignment_info());
  EXPECT_EQ(tasks->items()[0]->assignment_info()->surface_type(),
            TaskAssignmentInfo::SurfaceType::kDocument);

  EXPECT_EQ(tasks->items()[1]->id(), "space");
  ASSERT_TRUE(tasks->items()[1]->assignment_info());
  EXPECT_EQ(tasks->items()[1]->assignment_info()->surface_type(),
            TaskAssignmentInfo::SurfaceType::kSpace);

  EXPECT_EQ(tasks->items()[2]->id(), "unknown");
  ASSERT_TRUE(tasks->items()[2]->assignment_info());
  EXPECT_EQ(tasks->items()[2]->assignment_info()->surface_type(),
            TaskAssignmentInfo::SurfaceType::kUnknown);
}

TEST(TasksApiResponseTypesTest, FailsToCreateTasksFromInvalidResponse) {
  const auto raw_tasks = JSONReader::Read(R"(
      {
        "kind": "invalid_kind",
        "items": true
      })");
  ASSERT_TRUE(raw_tasks);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  ASSERT_FALSE(tasks);
}

TEST(TasksApiResponseTypesTest, CreatesTaskFromResponse) {
  const auto raw_task = JSONReader::Read(R"(
      {
        "kind": "tasks#task",
        "id": "asd",
        "title": "Parent task",
        "position": "00000000000000000001",
        "status": "needsAction",
        "due": "2023-04-19T00:00:00.000Z",
        "notes": "Lorem ipsum dolor sit amet"
      })");
  ASSERT_TRUE(raw_task);

  const auto task = Task::CreateFrom(*raw_task);
  ASSERT_TRUE(task);

  EXPECT_EQ(task->id(), "asd");
  EXPECT_EQ(task->title(), "Parent task");
  EXPECT_EQ(task->status(), TaskStatus::kNeedsAction);
  EXPECT_TRUE(task->parent_id().empty());
  EXPECT_EQ(task->position(), "00000000000000000001");
  EXPECT_EQ(util::FormatTimeAsString(task->due().value()),
            "2023-04-19T00:00:00.000Z");
  EXPECT_EQ(task->notes(), "Lorem ipsum dolor sit amet");
}

TEST(TasksApiResponseTypesTest, FailsToCreateTaskFromInvalidResponse) {
  const auto raw_task = JSONReader::Read(R"(
      {
        "kind": "invalid_kind",
        "id": true
      })");
  ASSERT_TRUE(raw_task);

  const auto task = Task::CreateFrom(*raw_task);
  ASSERT_FALSE(task);
}

}  // namespace google_apis::tasks
