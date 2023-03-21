// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_response_types.h"

#include <memory>

#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/test_util.h"
#include "google_apis/common/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::tasks {

TEST(TasksApiResponseTypesTest, CreatesTaskListsFromResponse) {
  const auto raw_task_lists = test_util::LoadJSONFile("tasks/task_lists.json");
  ASSERT_TRUE(raw_task_lists.get());
  ASSERT_EQ(raw_task_lists->type(), base::Value::Type::DICT);

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
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
  const auto raw_task_lists = test_util::LoadJSONFile("tasks/task_lists.json");
  ASSERT_TRUE(raw_task_lists.get());
  ASSERT_EQ(raw_task_lists->type(), base::Value::Type::DICT);

  raw_task_lists->SetStringKey("nextPageToken", "qwerty");

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
  EXPECT_EQ(task_lists->next_page_token(), "qwerty");
}

TEST(TasksApiResponseTypesTest, FailsToCreateTaskListsFromInvalidResponse) {
  const auto raw_task_lists = test_util::LoadJSONFile("tasks/task_lists.json");
  ASSERT_TRUE(raw_task_lists.get());
  ASSERT_EQ(raw_task_lists->type(), base::Value::Type::DICT);

  raw_task_lists->SetStringKey(kApiResponseKindKey, "invalid_kind");

  const auto task_lists = TaskLists::CreateFrom(*raw_task_lists);
  EXPECT_FALSE(task_lists);
}

TEST(TasksApiResponseTypesTest, CreatesTasksFromResponse) {
  const auto raw_tasks = test_util::LoadJSONFile("tasks/tasks.json");
  ASSERT_TRUE(raw_tasks.get());
  ASSERT_EQ(raw_tasks->type(), base::Value::Type::DICT);

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  EXPECT_TRUE(tasks->next_page_token().empty());
  EXPECT_EQ(tasks->items().size(), 2u);

  EXPECT_EQ(tasks->items()[0]->id(), "qwe");
  EXPECT_EQ(tasks->items()[0]->title(), "Completed child task");
  EXPECT_EQ(tasks->items()[0]->status(), Task::Status::kCompleted);
  EXPECT_EQ(tasks->items()[0]->parent_id(), "asd");

  EXPECT_EQ(tasks->items()[1]->id(), "asd");
  EXPECT_EQ(tasks->items()[1]->title(), "Parent task");
  EXPECT_EQ(tasks->items()[1]->status(), Task::Status::kNeedsAction);
  EXPECT_TRUE(tasks->items()[1]->parent_id().empty());
}

TEST(TasksApiResponseTypesTest, CreatesTasksWithNextPageTokenFromResponse) {
  const auto raw_tasks = test_util::LoadJSONFile("tasks/tasks.json");
  ASSERT_TRUE(raw_tasks.get());
  ASSERT_EQ(raw_tasks->type(), base::Value::Type::DICT);

  raw_tasks->SetStringKey("nextPageToken", "qwerty");

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  EXPECT_EQ(tasks->next_page_token(), "qwerty");
}

TEST(TasksApiResponseTypesTest, FailsToCreateTasksFromInvalidResponse) {
  const auto raw_tasks = test_util::LoadJSONFile("tasks/tasks.json");
  ASSERT_TRUE(raw_tasks.get());
  ASSERT_EQ(raw_tasks->type(), base::Value::Type::DICT);

  raw_tasks->SetStringKey(kApiResponseKindKey, "invalid_kind");

  const auto tasks = Tasks::CreateFrom(*raw_tasks);
  EXPECT_FALSE(tasks);
}

}  // namespace google_apis::tasks
