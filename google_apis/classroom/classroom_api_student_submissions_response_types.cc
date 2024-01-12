// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"

#include <string_view>

#include "base/json/json_value_converter.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"

namespace google_apis::classroom {
namespace {

constexpr char kApiResponseStudentSubmissionsItemsKey[] = "studentSubmissions";
constexpr char kApiResponseStudentSubmissionCourseWorkIdKey[] = "courseWorkId";
constexpr char kApiResponseStudentSubmissionStateKey[] = "state";
constexpr char kApiResponseStudentSubmissionAssignedGradeKey[] =
    "assignedGrade";
constexpr char kApiResponseStudentSubmissionUpdateTimeKey[] = "updateTime";

constexpr char kNewStudentSubmissionState[] = "NEW";
constexpr char kCreatedStudentSubmissionState[] = "CREATED";
constexpr char kTurnedInStudentSubmissionState[] = "TURNED_IN";
constexpr char kReturnedStudentSubmissionState[] = "RETURNED";
constexpr char kReclaimedStudentSubmissionState[] = "RECLAIMED_BY_STUDENT";

bool ConvertStudentSubmissionState(std::string_view input,
                                   StudentSubmission::State* output) {
  if (input == kNewStudentSubmissionState) {
    *output = StudentSubmission::State::kNew;
  } else if (input == kCreatedStudentSubmissionState) {
    *output = StudentSubmission::State::kCreated;
  } else if (input == kTurnedInStudentSubmissionState) {
    *output = StudentSubmission::State::kTurnedIn;
  } else if (input == kReturnedStudentSubmissionState) {
    *output = StudentSubmission::State::kReturned;
  } else if (input == kReclaimedStudentSubmissionState) {
    *output = StudentSubmission::State::kReclaimedByStudent;
  } else {
    *output = StudentSubmission::State::kOther;
  }
  return true;
}

bool ConvertAssignedGrade(const base::Value* input,
                          std::optional<double>* assigned_grade) {
  *assigned_grade = input->GetIfDouble();
  return true;
}

bool ConvertUpdateTime(std::string_view input,
                       std::optional<base::Time>* output) {
  base::Time update_time;
  if (!util::GetTimeFromString(input, &update_time)) {
    return false;
  }
  *output = update_time;
  return true;
}

}  // namespace

// ----- StudentSubmission -----

// static
void StudentSubmission::RegisterJSONConverter(
    base::JSONValueConverter<StudentSubmission>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &StudentSubmission::id_);
  converter->RegisterStringField(kApiResponseStudentSubmissionCourseWorkIdKey,
                                 &StudentSubmission::course_work_id_);
  converter->RegisterCustomField<StudentSubmission::State>(
      kApiResponseStudentSubmissionStateKey, &StudentSubmission::state_,
      &ConvertStudentSubmissionState);
  converter->RegisterCustomValueField<std::optional<double>>(
      kApiResponseStudentSubmissionAssignedGradeKey,
      &StudentSubmission::assigned_grade_, &ConvertAssignedGrade);
  converter->RegisterCustomField<std::optional<base::Time>>(
      kApiResponseStudentSubmissionUpdateTimeKey,
      &StudentSubmission::last_update_, &ConvertUpdateTime);
}

// ----- StudentSubmissions -----

StudentSubmissions::StudentSubmissions() = default;

StudentSubmissions::~StudentSubmissions() = default;

// static
void StudentSubmissions::RegisterJSONConverter(
    base::JSONValueConverter<StudentSubmissions>* converter) {
  converter->RegisterRepeatedMessage<StudentSubmission>(
      kApiResponseStudentSubmissionsItemsKey, &StudentSubmissions::items_);
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &StudentSubmissions::next_page_token_);
}

// static
std::unique_ptr<StudentSubmissions> StudentSubmissions::CreateFrom(
    const base::Value& value) {
  auto submissions = std::make_unique<StudentSubmissions>();
  base::JSONValueConverter<StudentSubmissions> converter;
  if (!converter.Convert(value, submissions.get())) {
    return nullptr;
  }
  return submissions;
}

}  // namespace google_apis::classroom
