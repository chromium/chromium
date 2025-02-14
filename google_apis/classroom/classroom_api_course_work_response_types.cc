// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_response_types.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/json/json_value_converter.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kApiResponseCourseWorkKey[] = "courseWork";
constexpr char kApiResponseCourseWorkItemAlternateLinkKey[] = "alternateLink";
constexpr char kApiResponseCourseWorkItemCreationTimeKey[] = "creationTime";
constexpr char kApiResponseCourseWorkItemUpdateTimeKey[] = "updateTime";
constexpr char kApiResponseCourseWorkItemDueDateKey[] = "dueDate";
constexpr char kApiResponseCourseWorkItemDueTimeKey[] = "dueTime";
constexpr char kApiResponseCourseWorkItemStateKey[] = "state";
constexpr char kApiResponseCourseWorkItemTitleKey[] = "title";
constexpr char kApiResponseCourseWorkItemMaterialsKey[] = "materials";
constexpr char kApiResponseCourseWorkItemTypeKey[] = "workType";

constexpr char kDueDateYearComponent[] = "year";
constexpr char kDueDateMonthComponent[] = "month";
constexpr char kDueDateDayComponent[] = "day";

constexpr char kDueTimeHoursComponent[] = "hours";
constexpr char kDueTimeMinutesComponent[] = "minutes";
constexpr char kDueTimeSecondsComponent[] = "seconds";
constexpr char kDueTimeNanosComponent[] = "nanos";

constexpr char kApiResponseCourseWorkItemMaterialDriveKey[] = "driveFile";
constexpr char kApiResponseCourseWorkItemMaterialYoutubeVideoKey[] =
    "youtubeVideo";
constexpr char kApiResponseCourseWorkItemMaterialLinkKey[] = "link";
constexpr char kApiResponseCourseWorkItemMaterialFormKey[] = "form";

constexpr char kPublishedCourseWorkItemState[] = "PUBLISHED";
constexpr char kAssignmentCourseWorkItemType[] = "ASSIGNMENT";
constexpr char kShortAnswerQuestionCourseWorkItemType[] =
    "SHORT_ANSWER_QUESTION";
constexpr char kMultipleChoiceQuestionCourseWorkItemType[] =
    "MULTIPLE_CHOICE_QUESTION";

bool ConvertCourseWorkItemState(std::string_view input,
                                CourseWorkItem::State* output) {
  *output = input == kPublishedCourseWorkItemState
                ? CourseWorkItem::State::kPublished
                : CourseWorkItem::State::kOther;
  return true;
}

bool ConvertCourseWorkItemType(std::string_view input,
                               CourseWorkItem::Type* output) {
  if (input == kAssignmentCourseWorkItemType) {
    *output = CourseWorkItem::Type::kAssignment;
  } else if (input == kShortAnswerQuestionCourseWorkItemType) {
    *output = CourseWorkItem::Type::kShortAnswerQuestion;
  } else if (input == kMultipleChoiceQuestionCourseWorkItemType) {
    *output = CourseWorkItem::Type::kMultipleChoiceQuestion;
  } else {
    *output = CourseWorkItem::Type::kUnspecified;
  }
  return true;
}

bool ConvertCourseWorkItemAlternateLink(std::string_view input, GURL* output) {
  *output = GURL(input);
  return true;
}

base::TimeDelta GetCourseWorkItemDueTime(
    const base::Value::Dict& raw_course_work_item) {
  const auto* const time =
      raw_course_work_item.FindDict(kApiResponseCourseWorkItemDueTimeKey);
  if (!time) {
    return base::TimeDelta();
  }

  const auto hours = time->FindInt(kDueTimeHoursComponent);
  const auto minutes = time->FindInt(kDueTimeMinutesComponent);
  const auto seconds = time->FindInt(kDueTimeSecondsComponent);
  const auto nanos = time->FindInt(kDueTimeNanosComponent);

  return base::Hours(hours.value_or(0)) + base::Minutes(minutes.value_or(0)) +
         base::Seconds(seconds.value_or(0)) +
         base::Nanoseconds(nanos.value_or(0));
}

std::optional<CourseWorkItem::DueDateTime> GetCourseWorkItemDueDateTime(
    const base::Value::Dict& raw_course_work_item) {
  const auto* const date =
      raw_course_work_item.FindDict(kApiResponseCourseWorkItemDueDateKey);
  if (!date) {
    return std::nullopt;
  }

  const auto year = date->FindInt(kDueDateYearComponent);
  const auto month = date->FindInt(kDueDateMonthComponent);
  const auto day = date->FindInt(kDueDateDayComponent);

  if (!year.has_value() && !month.has_value() && !day.has_value()) {
    return std::nullopt;
  }

  return CourseWorkItem::DueDateTime{
      .year = year.value_or(0),
      .month = month.value_or(0),
      .day = day.value_or(0),
      .time_of_day = GetCourseWorkItemDueTime(raw_course_work_item)};
}

}  // namespace

// ----- Material -----

Material::Material() = default;

Material::~Material() = default;

// static
bool Material::ConvertMaterial(const base::Value* input, Material* output) {
  const base::Value::Dict* dict = input->GetIfDict();
  if (!dict) {
    return false;
  }

  const auto* const sharedDriveFile =
      dict->FindDict(kApiResponseCourseWorkItemMaterialDriveKey);
  const auto* const youtubeVideo =
      dict->FindDict(kApiResponseCourseWorkItemMaterialYoutubeVideoKey);
  const auto* const link =
      dict->FindDict(kApiResponseCourseWorkItemMaterialLinkKey);
  const auto* const form =
      dict->FindDict(kApiResponseCourseWorkItemMaterialFormKey);
  if (sharedDriveFile) {
    const auto* const driveFile =
        sharedDriveFile->FindDict(kApiResponseCourseWorkItemMaterialDriveKey);
    if (!driveFile) {
      // Shared drive file should contain a drive file.
      return false;
    }
    const std::string* title =
        driveFile->FindString(kApiResponseCourseWorkItemTitleKey);
    if (!title) {
      // Title is required field.
      return false;
    }
    output->title_ = *title;
    output->type_ = Material::Type::kSharedDriveFile;
  } else if (youtubeVideo) {
    const std::string* title =
        youtubeVideo->FindString(kApiResponseCourseWorkItemTitleKey);
    if (!title) {
      // Title is required field.
      return false;
    }
    output->title_ = *title;
    output->type_ = Material::Type::kYoutubeVideo;
  } else if (link) {
    const std::string* title =
        link->FindString(kApiResponseCourseWorkItemTitleKey);
    if (!title) {
      // Title is required field.
      return false;
    }
    output->title_ = *title;
    output->type_ = Material::Type::kLink;
  } else if (form) {
    const std::string* title =
        form->FindString(kApiResponseCourseWorkItemTitleKey);
    if (!title) {
      // Title is required field.
      return false;
    }
    output->title_ = *title;
    output->type_ = Material::Type::kForm;
  } else {
    output->type_ = Material::Type::kUnknown;
  }

  return true;
}

// ----- CourseWorkItem -----

CourseWorkItem::CourseWorkItem() = default;

CourseWorkItem::~CourseWorkItem() = default;

// static
void CourseWorkItem::RegisterJSONConverter(
    base::JSONValueConverter<CourseWorkItem>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &CourseWorkItem::id_);
  converter->RegisterStringField(kApiResponseCourseWorkItemTitleKey,
                                 &CourseWorkItem::title_);
  converter->RegisterCustomField<CourseWorkItem::State>(
      kApiResponseCourseWorkItemStateKey, &CourseWorkItem::state_,
      &ConvertCourseWorkItemState);
  converter->RegisterCustomField<GURL>(
      kApiResponseCourseWorkItemAlternateLinkKey,
      &CourseWorkItem::alternate_link_, &ConvertCourseWorkItemAlternateLink);
  converter->RegisterCustomField<base::Time>(
      kApiResponseCourseWorkItemCreationTimeKey,
      &CourseWorkItem::creation_time_, &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kApiResponseCourseWorkItemUpdateTimeKey, &CourseWorkItem::last_update_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<CourseWorkItem::Type>(
      kApiResponseCourseWorkItemTypeKey, &CourseWorkItem::type_,
      &ConvertCourseWorkItemType);
  converter->RegisterRepeatedCustomValue<Material>(
      kApiResponseCourseWorkItemMaterialsKey, &CourseWorkItem::materials_,
      &Material::ConvertMaterial);
}

// static
bool CourseWorkItem::ConvertCourseWorkItem(const base::Value* input,
                                           CourseWorkItem* output) {
  base::JSONValueConverter<CourseWorkItem> converter;
  const base::Value::Dict* dict = input->GetIfDict();
  if (!dict || !converter.Convert(*dict, output)) {
    return false;
  }

  output->due_date_time_ = GetCourseWorkItemDueDateTime(*dict);
  return true;
}

// ----- CourseWork -----

CourseWork::CourseWork() = default;

CourseWork::~CourseWork() = default;

// static
void CourseWork::RegisterJSONConverter(
    base::JSONValueConverter<CourseWork>* converter) {
  // TODO(crbug.com/40911919): Handle base::Value::Dict here.
  converter->RegisterRepeatedCustomValue<CourseWorkItem>(
      kApiResponseCourseWorkKey, &CourseWork::items_,
      &CourseWorkItem::ConvertCourseWorkItem);
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &CourseWork::next_page_token_);
}

// static
std::unique_ptr<CourseWork> CourseWork::CreateFrom(const base::Value& value) {
  auto course_work = std::make_unique<CourseWork>();
  base::JSONValueConverter<CourseWork> converter;
  if (!converter.Convert(value, course_work.get())) {
    return nullptr;
  }
  return course_work;
}

}  // namespace google_apis::classroom
