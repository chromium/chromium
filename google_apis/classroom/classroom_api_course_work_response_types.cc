// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_response_types.h"

#include <memory>
#include <string>

#include "base/json/json_value_converter.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kApiResponseCourseWorkKey[] = "courseWork";
constexpr char kApiResponseCourseWorkItemAlternateLinkKey[] = "alternateLink";
constexpr char kApiResponseCourseWorkItemStateKey[] = "state";
constexpr char kApiResponseCourseWorkItemTitleKey[] = "title";

constexpr char kPublishedCourseWorkItemState[] = "PUBLISHED";

bool ConvertCourseWorkItemState(base::StringPiece input,
                                CourseWorkItem::State* output) {
  *output = input == kPublishedCourseWorkItemState
                ? CourseWorkItem::State::kPublished
                : CourseWorkItem::State::kOther;
  return true;
}

bool ConvertCourseWorkItemAlternateLink(base::StringPiece input, GURL* output) {
  *output = GURL(input);
  return true;
}

}  // namespace

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
}

// ----- CourseWork -----

CourseWork::CourseWork() = default;

CourseWork::~CourseWork() = default;

// static
void CourseWork::RegisterJSONConverter(
    base::JSONValueConverter<CourseWork>* converter) {
  converter->RegisterRepeatedMessage<CourseWorkItem>(kApiResponseCourseWorkKey,
                                                     &CourseWork::items_);
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
