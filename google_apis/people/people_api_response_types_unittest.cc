// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_response_types.h"

#include "base/json/json_value_converter.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::people {
namespace {

TEST(PeopleApiResponseTypesTest, PersonValueConverterParsesResourceName) {
  base::JSONValueConverter<Person> converter;

  Person person;
  base::Value json = base::test::ParseJson(R"json({
    "resourceName": "people/c12345",
  })json");
  converter.Convert(json, &person);

  EXPECT_EQ(person.resource_name, "people/c12345");
}

}  // namespace
}  // namespace google_apis::people
