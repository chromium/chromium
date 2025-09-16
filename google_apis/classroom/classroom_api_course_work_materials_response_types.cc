// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_materials_response_types.h"

#include <memory>
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

// JSON response keys.
constexpr char kApiResponseCourseWorkMaterialKey[] = "courseWorkMaterial";
constexpr char kApiResponseItemAlternateLinkKey[] = "alternateLink";
constexpr char kApiResponseItemCreationTimeKey[] = "creationTime";
constexpr char kApiResponseItemUpdateTimeKey[] = "updateTime";
constexpr char kApiResponseItemStateKey[] = "state";
constexpr char kApiResponseItemTitleKey[] = "title";
constexpr char kApiResponseItemMaterialsKey[] = "materials";

// State values.
constexpr char kPublishedState[] = "PUBLISHED";

bool ConvertState(std::string_view input,
                  CourseWorkMaterialItem::State* output) {
  *output = input == kPublishedState ? CourseWorkMaterialItem::State::kPublished
                                     : CourseWorkMaterialItem::State::kOther;
  return true;
}

bool ConvertAlternateLink(std::string_view input, GURL* output) {
  *output = GURL(input);
  return true;
}

}  // namespace

CourseWorkMaterialItem::CourseWorkMaterialItem() = default;
CourseWorkMaterialItem::~CourseWorkMaterialItem() = default;

void CourseWorkMaterialItem::RegisterJSONConverter(
    base::JSONValueConverter<CourseWorkMaterialItem>* converter) {
  converter->RegisterStringField(kApiResponseIdKey,
                                 &CourseWorkMaterialItem::id_);
  converter->RegisterStringField(kApiResponseItemTitleKey,
                                 &CourseWorkMaterialItem::title_);
  converter->RegisterCustomField<CourseWorkMaterialItem::State>(
      kApiResponseItemStateKey, &CourseWorkMaterialItem::state_, &ConvertState);
  converter->RegisterCustomField<GURL>(kApiResponseItemAlternateLinkKey,
                                       &CourseWorkMaterialItem::alternate_link_,
                                       &ConvertAlternateLink);
  converter->RegisterCustomField<base::Time>(
      kApiResponseItemCreationTimeKey, &CourseWorkMaterialItem::creation_time_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kApiResponseItemUpdateTimeKey, &CourseWorkMaterialItem::last_update_,
      &util::GetTimeFromString);
  converter->RegisterRepeatedCustomValue<Material>(
      kApiResponseItemMaterialsKey, &CourseWorkMaterialItem::materials_,
      &Material::ConvertMaterial);
}

CourseWorkMaterial::CourseWorkMaterial() = default;
CourseWorkMaterial::~CourseWorkMaterial() = default;

void CourseWorkMaterial::RegisterJSONConverter(
    base::JSONValueConverter<CourseWorkMaterial>* converter) {
  converter->RegisterRepeatedMessage<CourseWorkMaterialItem>(
      kApiResponseCourseWorkMaterialKey, &CourseWorkMaterial::items_);
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &CourseWorkMaterial::next_page_token_);
}

std::unique_ptr<CourseWorkMaterial> CourseWorkMaterial::CreateFrom(
    const base::Value& value) {
  auto course_work_material = std::make_unique<CourseWorkMaterial>();
  base::JSONValueConverter<CourseWorkMaterial> converter;
  if (!converter.Convert(value, course_work_material.get())) {
    return nullptr;
  }
  return course_work_material;
}

}  // namespace google_apis::classroom
