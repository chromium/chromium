// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_response_types.h"

#include <memory>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace google_apis::tasks {
namespace {

using ::base::JSONValueConverter;

constexpr char kTaskListsKind[] = "tasks#taskLists";
constexpr char kTasksKind[] = "tasks#tasks";

constexpr char kApiResponseParentKey[] = "parent";
constexpr char kApiResponseStatusKey[] = "status";
constexpr char kApiResponseTitleKey[] = "title";
constexpr char kApiResponseUpdatedKey[] = "updated";

constexpr auto kTaskStatuses =
    base::MakeFixedFlatMap<base::StringPiece, Task::Status>(
        {{"needsAction", Task::Status::kNeedsAction},
         {"completed", Task::Status::kCompleted}});

bool ConvertTaskStatus(base::StringPiece input, Task::Status* output) {
  *output = kTaskStatuses.contains(input) ? kTaskStatuses.at(input)
                                          : Task::Status::kUnknown;
  return true;
}

bool ConvertToOptionalString(base::StringPiece input,
                             absl::optional<std::string>* output) {
  *output = std::string(input);
  return true;
}

}  // namespace

// ----- TaskList -----

TaskList::TaskList() = default;
TaskList::~TaskList() = default;

void TaskList::RegisterJSONConverter(JSONValueConverter<TaskList>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &TaskList::id_);
  converter->RegisterStringField(kApiResponseTitleKey, &TaskList::title_);
  converter->RegisterCustomField<base::Time>(
      kApiResponseUpdatedKey, &TaskList::updated_, &util::GetTimeFromString);
}

// ----- TaskLists -----

TaskLists::TaskLists() = default;
TaskLists::~TaskLists() = default;

void TaskLists::RegisterJSONConverter(
    JSONValueConverter<TaskLists>* converter) {
  converter->RegisterRepeatedMessage<TaskList>(kApiResponseItemsKey,
                                               &TaskLists::items_);
}

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

// ----- Task -----

Task::Task() = default;
Task::~Task() = default;

void Task::RegisterJSONConverter(JSONValueConverter<Task>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &Task::id_);
  converter->RegisterStringField(kApiResponseTitleKey, &Task::title_);
  converter->RegisterCustomField<Task::Status>(
      kApiResponseStatusKey, &Task::status_, &ConvertTaskStatus);
  converter->RegisterCustomField<absl::optional<std::string>>(
      kApiResponseParentKey, &Task::parent_id_, &ConvertToOptionalString);
}

// ----- Tasks -----

Tasks::Tasks() = default;
Tasks::~Tasks() = default;

void Tasks::RegisterJSONConverter(JSONValueConverter<Tasks>* converter) {
  converter->RegisterRepeatedMessage<Task>(kApiResponseItemsKey,
                                           &Tasks::items_);
}

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
