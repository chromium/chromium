// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "url/gurl.h"

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

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  const base::Time& updated() const { return updated_; }

 private:
  // Task list identifier.
  std::string id_;

  // Title of the task list.
  std::string title_;

  // Last modification time of the task list.
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

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<TaskList>>& items() const { return items_; }
  std::vector<std::unique_ptr<TaskList>>* mutable_items() { return &items_; }

 private:
  // Token that can be used to request the next page of this result.
  std::string next_page_token_;

  // `TaskList` items stored in this container.
  std::vector<std::unique_ptr<TaskList>> items_;
};

// https://developers.google.com/tasks/reference/rest/v1/tasks (see "links[]*").
class TaskLink {
 public:
  // Type of the link.
  enum class Type {
    kEmail,  // is the only supported right now.
    kUnknown,
  };

  TaskLink();
  TaskLink(const TaskLink&) = delete;
  TaskLink& operator=(const TaskLink&) = delete;
  ~TaskLink();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskLink>* converter);

  Type type() const { return type_; }

 private:
  // Type of the link.
  Type type_ = Type::kUnknown;
};

// https://developers.google.com/tasks/reference/rest/v1/tasks (see
// "assignmentInfo").
class TaskAssignmentInfo {
 public:
  // The type of surface an assigned task originates from.
  enum class SurfaceType {
    // The task is assigned from a document.
    kDocument,

    // The task is assigned from a Chat Space.
    kSpace,

    // Default / fallback option for unknown values.
    kUnknown,
  };

  TaskAssignmentInfo();
  TaskAssignmentInfo(const TaskAssignmentInfo&) = delete;
  TaskAssignmentInfo& operator=(const TaskAssignmentInfo&) = delete;
  ~TaskAssignmentInfo();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskAssignmentInfo>* converter);

  SurfaceType surface_type() const { return surface_type_; }

 private:
  // The type of surface this assigned task originates from.
  SurfaceType surface_type_ = SurfaceType::kUnknown;
};

// https://developers.google.com/tasks/reference/rest/v1/tasks
class Task {
 public:
  Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Task>* converter);

  // Creates a `Task` from parsed JSON.
  static std::unique_ptr<Task> CreateFrom(const base::Value& value);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  TaskStatus status() const { return status_; }
  const std::string& parent_id() const { return parent_id_; }
  const std::string& position() const { return position_; }
  const std::optional<base::Time>& due() const { return due_; }
  const std::vector<std::unique_ptr<TaskLink>>& links() const { return links_; }
  const std::string& notes() const { return notes_; }
  const base::Time& updated() const { return updated_; }
  const GURL& web_view_link() const { return web_view_link_; }
  const std::optional<TaskAssignmentInfo>& assignment_info() const {
    return assignment_info_;
  }

 private:
  // Task identifier.
  std::string id_;

  // Title of the task.
  std::string title_;

  // Status of the task.
  TaskStatus status_ = TaskStatus::kUnknown;

  // Parent task identifier.
  std::string parent_id_;

  // Position of the task among its sibling tasks.
  std::string position_;

  // Due date of the task (comes as a RFC 3339 timestamp and converted to
  // `base::Time`). The due date only records date information. Not all tasks
  // have a due date.
  std::optional<base::Time> due_ = std::nullopt;

  // Collection of links related to this task.
  std::vector<std::unique_ptr<TaskLink>> links_;

  // Notes describing the task.
  std::string notes_;

  // When the task was last updated.
  base::Time updated_;

  // Absolute link to the task in the Google Tasks Web UI.
  GURL web_view_link_;

  // Information about the source of the task assignment.
  std::optional<TaskAssignmentInfo> assignment_info_;
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

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<Task>>& items() const { return items_; }
  std::vector<std::unique_ptr<Task>>* mutable_items() { return &items_; }

 private:
  // Token that can be used to request the next page of this result.
  std::string next_page_token_;

  // `Task` items stored in this container.
  std::vector<std::unique_ptr<Task>> items_;
};

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
