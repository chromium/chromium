// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_courses_response_types.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/json/json_value_converter.h"
#include "base/notreached.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"

namespace google_apis::classroom {
namespace {

constexpr char kApiResponseCoursesKey[] = "courses";
constexpr char kApiResponseCourseStateKey[] = "courseState";

constexpr char kActiveCourseState[] = "ACTIVE";

bool ConvertCourseState(std::string_view input, Course::State* output) {
  *output = input == kActiveCourseState ? Course::State::kActive
                                        : Course::State::kOther;
  return true;
}

}  // namespace

// ----- Course -----

// static
void Course::RegisterJSONConverter(
    base::JSONValueConverter<Course>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &Course::id_);
  converter->RegisterStringField(kApiResponseNameKey, &Course::name_);
  converter->RegisterCustomField<Course::State>(
      kApiResponseCourseStateKey, &Course::state_, &ConvertCourseState);
}

// static
std::string Course::StateToString(Course::State state) {
  if (state == Course::State::kActive) {
    return kActiveCourseState;
  }
  NOTREACHED();
}

// ----- Courses -----

Courses::Courses() = default;

Courses::~Courses() = default;

// static
void Courses::RegisterJSONConverter(
    base::JSONValueConverter<Courses>* converter) {
  converter->RegisterRepeatedMessage<Course>(kApiResponseCoursesKey,
                                             &Courses::items_);
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &Courses::next_page_token_);
}

// static
std::unique_ptr<Courses> Courses::CreateFrom(const base::Value& value) {
  auto courses = std::make_unique<Courses>();
  base::JSONValueConverter<Courses> converter;
  if (!converter.Convert(value, courses.get())) {
    return nullptr;
  }
  return courses;
}

}  // namespace google_apis::classroom
