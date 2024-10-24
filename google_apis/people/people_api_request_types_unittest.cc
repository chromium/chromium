// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_request_types.h"

#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::people {
namespace {

using ::base::test::IsJson;

TEST(PeopleApiRequestTypesTest, NameWithNoFieldsToDict) {
  Name name;

  base::Value::Dict dict = std::move(name).ToDict();

  EXPECT_THAT(dict, IsJson("{}"));
}

TEST(PeopleApiRequestTypesTest, NameWithFamilyNameToDict) {
  Name name;
  name.family_name = "Francois";

  base::Value::Dict dict = std::move(name).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "familyName": "Francois",
  })json"));
}

TEST(PeopleApiRequestTypesTest, NameWithGivenNameToDict) {
  Name name;
  name.given_name = "Andre";

  base::Value::Dict dict = std::move(name).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "givenName": "Andre",
  })json"));
}

TEST(PeopleApiRequestTypesTest, NameWithMultipleFieldsToDict) {
  Name name;
  name.family_name = "Francois";
  name.given_name = "Andre";

  base::Value::Dict dict = std::move(name).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "familyName": "Francois",
    "givenName": "Andre",
  })json"));
}

TEST(PeopleApiRequestTypesTest, ContactWithNoFieldsToDict) {
  Contact contact;

  base::Value::Dict dict = std::move(contact).ToDict();

  EXPECT_THAT(dict, IsJson("{}"));
}

TEST(PeopleApiRequestTypesTest, ContactWithNameToDict) {
  Contact contact;
  contact.name.family_name = "Francois";
  contact.name.given_name = "Andre";

  base::Value::Dict dict = std::move(contact).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "names": [
      {
        "familyName": "Francois",
        "givenName": "Andre",
      },
    ],
  })json"));
}

}  // namespace
}  // namespace google_apis::people
