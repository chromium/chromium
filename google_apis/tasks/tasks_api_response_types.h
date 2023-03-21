// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::tasks {

// https://developers.google.com/tasks/reference/rest/v1/tasklists
class TaskList {
 public:
  TaskList();
  TaskList(const TaskList&) = delete;
  TaskList& operator=(const TaskList&) = delete;
  ~TaskList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskList>* converter);

  // Task list identifier.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // Title of the task list.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // Last modification time of the task list.
  const base::Time& updated() const { return updated_; }
  void set_updated(const base::Time& updated) { updated_ = updated; }

 private:
  std::string id_;
  std::string title_;
  base::Time updated_;
};

// Container for multiple `TaskList`s.
class TaskLists {
 public:
  TaskLists();
  TaskLists(const TaskLists&) = delete;
  TaskLists& operator=(const TaskLists&) = delete;
  ~TaskLists();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskLists>* converter);

  // Creates a `TaskLists` from parsed JSON.
  static std::unique_ptr<TaskLists> CreateFrom(const base::Value& value);

  // Returns a token that can be used to request the next page of this result.
  const std::string& next_page_token() const { return next_page_token_; }
  void set_next_page_token(const std::string& next_page_token) {
    next_page_token_ = next_page_token;
  }

  // Returns `TaskList` items stored in this container.
  const std::vector<std::unique_ptr<TaskList>>& items() const { return items_; }
  std::vector<std::unique_ptr<TaskList>>* mutable_items() { return &items_; }

 private:
  std::string next_page_token_;
  std::vector<std::unique_ptr<TaskList>> items_;
};

// https://developers.google.com/tasks/reference/rest/v1/tasks
class Task {
 public:
  Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  // Status of the task.
  enum class Status {
    kUnknown,
    kNeedsAction,
    kCompleted,
  };

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Task>* converter);

  // Task identifier.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // Title of the task.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // Status of the task.
  Status status() const { return status_; }
  void set_status(Status status) { status_ = status; }

  // Parent task identifier.
  const std::string& parent_id() const { return parent_id_; }
  void set_parent_id(const std::string& parent_id) { parent_id_ = parent_id; }

 private:
  std::string id_;
  std::string title_;
  Status status_ = Status::kUnknown;
  std::string parent_id_;
};

// Container for multiple `Task`s.
class Tasks {
 public:
  Tasks();
  Tasks(const Tasks&) = delete;
  Tasks& operator=(const Tasks&) = delete;
  ~Tasks();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Tasks>* converter);

  // Creates a `Tasks` from parsed JSON.
  static std::unique_ptr<Tasks> CreateFrom(const base::Value& value);

  // Returns a token that can be used to request the next page of this result.
  const std::string& next_page_token() const { return next_page_token_; }
  void set_next_page_token(const std::string& next_page_token) {
    next_page_token_ = next_page_token;
  }

  // Returns `Task` items stored in this container.
  const std::vector<std::unique_ptr<Task>>& items() const { return items_; }
  std::vector<std::unique_ptr<Task>>* mutable_items() { return &items_; }

 private:
  std::string next_page_token_;
  std::vector<std::unique_ptr<Task>> items_;
};

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
