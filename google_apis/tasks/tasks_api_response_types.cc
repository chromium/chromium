// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_response_types.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "url/gurl.h"

namespace google_apis::tasks {
namespace {

using ::base::JSONValueConverter;

constexpr char kTaskKind[] = "tasks#task";
constexpr char kTaskListsKind[] = "tasks#taskLists";
constexpr char kTasksKind[] = "tasks#tasks";

constexpr char kApiResponseAssignmentInfoKey[] = "assignmentInfo";
constexpr char kApiResponseAssignmentInfoSurfaceTypeKey[] = "surfaceType";
constexpr char kApiResponseDueKey[] = "due";
constexpr char kApiResponseLinksKey[] = "links";
constexpr char kApiResponseLinkTypeKey[] = "type";
constexpr char kApiResponseNotesKey[] = "notes";
constexpr char kApiResponseParentKey[] = "parent";
constexpr char kApiResponsePositionKey[] = "position";
constexpr char kApiResponseStatusKey[] = "status";
constexpr char kApiResponseTitleKey[] = "title";
constexpr char kApiResponseUpdatedKey[] = "updated";
constexpr char kApiResponseWebViewLinkKey[] = "webViewLink";

// Known values of `kApiResponseAssignmentInfoSurfaceTypeKey`.
constexpr char kAssignmentInfoSurfaceTypeDocument[] = "DOCUMENT";
constexpr char kAssignmentInfoSurfaceTypeSpace[] = "SPACE";

// Known values of `kApiResponseLinkTypeKey`.
constexpr char kLinkTypeEmail[] = "email";

bool ConvertTaskStatus(std::string_view input, TaskStatus* output) {
  *output = TaskStatusFromString(input);
  return true;
}

bool ConvertTaskDueDate(std::string_view input,
                        std::optional<base::Time>* output) {
  base::Time due;
  if (!util::GetTimeFromString(input, &due)) {
    return false;
  }
  *output = due;
  return true;
}

bool ConvertTaskLinkType(std::string_view input, TaskLink::Type* output) {
  *output = input == kLinkTypeEmail ? TaskLink::Type::kEmail
                                    : TaskLink::Type::kUnknown;
  return true;
}

bool ConvertTaskWebViewLink(std::string_view input, GURL* output) {
  *output = GURL(input);
  return true;
}

bool ConvertTaskAssignmentInfo(const base::Value* input,
                               std::optional<TaskAssignmentInfo>* output) {
  if (!input) {
    return true;
  }

  output->emplace();
  JSONValueConverter<TaskAssignmentInfo> converter;
  if (!converter.Convert(*input, &output->value())) {
    DVLOG(1) << "Unable to construct `TaskAssignmentInfo` from parsed json.";
    output->reset();
    return false;
  }
  return true;
}

bool ConvertAssignmentInfoSurfaceType(std::string_view input,
                                      TaskAssignmentInfo::SurfaceType* output) {
  if (input == kAssignmentInfoSurfaceTypeDocument) {
    *output = TaskAssignmentInfo::SurfaceType::kDocument;
  } else if (input == kAssignmentInfoSurfaceTypeSpace) {
    *output = TaskAssignmentInfo::SurfaceType::kSpace;
  } else {
    *output = TaskAssignmentInfo::SurfaceType::kUnknown;
  }
  return true;
}

}  // namespace

// ----- TaskList -----

TaskList::TaskList() = default;
TaskList::~TaskList() = default;

// static
void TaskList::RegisterJSONConverter(JSONValueConverter<TaskList>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &TaskList::id_);
  converter->RegisterStringField(kApiResponseTitleKey, &TaskList::title_);
  converter->RegisterCustomField(kApiResponseUpdatedKey, &TaskList::updated_,
                                 &util::GetTimeFromString);
}

// ----- TaskLists -----

TaskLists::TaskLists() = default;
TaskLists::~TaskLists() = default;

// static
void TaskLists::RegisterJSONConverter(
    JSONValueConverter<TaskLists>* converter) {
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &TaskLists::next_page_token_);
  converter->RegisterRepeatedMessage(kApiResponseItemsKey, &TaskLists::items_);
}

// static
std::unique_ptr<TaskLists> TaskLists::CreateFrom(const base::Value& value) {
  auto task_lists = std::make_unique<TaskLists>();
  JSONValueConverter<TaskLists> converter;
  if (!IsResourceKindExpected(value, kTaskListsKind) ||
      !converter.Convert(value, task_lists.get())) {
    DVLOG(1) << "Unable to construct `TaskLists` from parsed json.";
    return nullptr;
  }
  return task_lists;
}

// ----- TaskLink -----

TaskLink::TaskLink() = default;
TaskLink::~TaskLink() = default;

// static
void TaskLink::RegisterJSONConverter(JSONValueConverter<TaskLink>* converter) {
  converter->RegisterCustomField(kApiResponseLinkTypeKey, &TaskLink::type_,
                                 &ConvertTaskLinkType);
}

// ----- TaskAssignmentInfo -----

TaskAssignmentInfo::TaskAssignmentInfo() = default;
TaskAssignmentInfo::~TaskAssignmentInfo() = default;

// static
void TaskAssignmentInfo::RegisterJSONConverter(
    JSONValueConverter<TaskAssignmentInfo>* converter) {
  converter->RegisterCustomField(kApiResponseAssignmentInfoSurfaceTypeKey,
                                 &TaskAssignmentInfo::surface_type_,
                                 &ConvertAssignmentInfoSurfaceType);
}

// ----- Task -----

Task::Task() = default;
Task::~Task() = default;

// static
void Task::RegisterJSONConverter(JSONValueConverter<Task>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &Task::id_);
  converter->RegisterStringField(kApiResponseTitleKey, &Task::title_);
  converter->RegisterCustomField(kApiResponseStatusKey, &Task::status_,
                                 &ConvertTaskStatus);
  converter->RegisterStringField(kApiResponseParentKey, &Task::parent_id_);
  converter->RegisterStringField(kApiResponsePositionKey, &Task::position_);
  converter->RegisterCustomField(kApiResponseDueKey, &Task::due_,
                                 &ConvertTaskDueDate);
  converter->RegisterRepeatedMessage(kApiResponseLinksKey, &Task::links_);
  converter->RegisterStringField(kApiResponseNotesKey, &Task::notes_);
  converter->RegisterCustomField(kApiResponseUpdatedKey, &Task::updated_,
                                 &util::GetTimeFromString);
  converter->RegisterCustomField(kApiResponseWebViewLinkKey,
                                 &Task::web_view_link_,
                                 &ConvertTaskWebViewLink);
  converter->RegisterCustomValueField(kApiResponseAssignmentInfoKey,
                                      &Task::assignment_info_,
                                      &ConvertTaskAssignmentInfo);
}

// static
std::unique_ptr<Task> Task::CreateFrom(const base::Value& value) {
  auto task = std::make_unique<Task>();
  JSONValueConverter<Task> converter;
  if (!IsResourceKindExpected(value, kTaskKind) ||
      !converter.Convert(value, task.get())) {
    DVLOG(1) << "Unable to construct a `Task` from parsed json.";
    return nullptr;
  }
  return task;
}

// ----- Tasks -----

Tasks::Tasks() = default;
Tasks::~Tasks() = default;

// static
void Tasks::RegisterJSONConverter(JSONValueConverter<Tasks>* converter) {
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &Tasks::next_page_token_);
  converter->RegisterRepeatedMessage(kApiResponseItemsKey, &Tasks::items_);
}

// static
std::unique_ptr<Tasks> Tasks::CreateFrom(const base::Value& value) {
  auto tasks = std::make_unique<Tasks>();
  JSONValueConverter<Tasks> converter;
  if (!IsResourceKindExpected(value, kTasksKind) ||
      !converter.Convert(value, tasks.get())) {
    DVLOG(1) << "Unable to construct `Tasks` from parsed json.";
    return nullptr;
  }
  return tasks;
}

}  // namespace google_apis::tasks
