// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_students_response_types.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/json/json_value_converter.h"
#include "base/notreached.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"

namespace google_apis::classroom {
namespace {

constexpr char kApiResponseProfileKey[] = "profile";
constexpr char kApiResponseStudentsKey[] = "students";
constexpr char kApiResponseFullNameKey[] = "fullName";
constexpr char kApiResponseEmailAddressKey[] = "emailAddress";
constexpr char kApiResponseNameKey[] = "name";
constexpr char kApiResponsePhotoUrlKey[] = "photoUrl";

// Converts |url_string| to |result|.  Always returns true to be used
// for JSONValueConverter::RegisterCustomField method.
bool GetGURLFromString(std::string_view url_string, GURL* result) {
  *result = GURL("https:" + std::string(url_string));
  return true;
}

}  // namespace

// ----- Name -----

// static
void Name::RegisterJSONConverter(base::JSONValueConverter<Name>* converter) {
  converter->RegisterStringField(kApiResponseFullNameKey, &Name::full_name_);
}

// ----- UserProfile -----

UserProfile::UserProfile() = default;
UserProfile::~UserProfile() = default;

// static
void UserProfile::RegisterJSONConverter(
    base::JSONValueConverter<UserProfile>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &UserProfile::id_);
  converter->RegisterNestedField<Name>(kApiResponseNameKey,
                                       &UserProfile::name_);
  converter->RegisterStringField(kApiResponseEmailAddressKey,
                                 &UserProfile::email_address_);
  converter->RegisterCustomField<GURL>(
      kApiResponsePhotoUrlKey, &UserProfile::photo_url_, GetGURLFromString);
}

// ----- Student -----

// static
void Student::RegisterJSONConverter(
    base::JSONValueConverter<Student>* converter) {
  converter->RegisterNestedField(kApiResponseProfileKey, &Student::profile_);
}

// ----- Students -----

Students::Students() = default;

Students::~Students() = default;

// static
void Students::RegisterJSONConverter(
    base::JSONValueConverter<Students>* converter) {
  converter->RegisterRepeatedMessage<Student>(kApiResponseStudentsKey,
                                              &Students::items_);
  converter->RegisterStringField(kApiResponseNextPageTokenKey,
                                 &Students::next_page_token_);
}

// static
std::unique_ptr<Students> Students::CreateFrom(const base::Value& value) {
  auto students = std::make_unique<Students>();
  base::JSONValueConverter<Students> converter;
  if (!converter.Convert(value, students.get())) {
    return nullptr;
  }
  return students;
}

}  // namespace google_apis::classroom
