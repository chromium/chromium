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

TEST(PeopleApiRequestTypesTest, EmailWithNoFieldsToDict) {
  EmailAddress email;

  base::Value::Dict dict = std::move(email).ToDict();

  EXPECT_THAT(dict, IsJson("{}"));
}

TEST(PeopleApiRequestTypesTest, EmailWithOnlyValueToDict) {
  EmailAddress email;
  email.value = "afrancois@example.com";

  base::Value::Dict dict = std::move(email).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "value": "afrancois@example.com",
  })json"));
}

TEST(PeopleApiRequestTypesTest, EmailWithMultipleFieldsToDict) {
  EmailAddress email;
  email.value = "afrancois@example.com";
  email.type = "home";

  base::Value::Dict dict = std::move(email).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "value": "afrancois@example.com",
    "type": "home",
  })json"));
}

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

TEST(PeopleApiRequestTypesTest, ContactWithOneEmailToDict) {
  Contact contact;
  EmailAddress email;
  email.value = "afrancois@example.com";
  contact.email_addresses.push_back(std::move(email));

  base::Value::Dict dict = std::move(contact).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
      },
    ],
  })json"));
}

TEST(PeopleApiRequestTypesTest, ContactWithMultipleEmailsToDict) {
  Contact contact;
  EmailAddress home_email;
  home_email.value = "afrancois@example.com";
  home_email.type = "home";
  contact.email_addresses.push_back(std::move(home_email));
  EmailAddress work_email;
  work_email.value = "afrancois@work.example.com";
  work_email.type = "work";
  contact.email_addresses.push_back(std::move(work_email));

  base::Value::Dict dict = std::move(contact).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
  })json"));
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

TEST(PeopleApiRequestTypesTest, ContactWithMultipleFieldsToDict) {
  Contact contact;
  EmailAddress home_email;
  home_email.value = "afrancois@example.com";
  home_email.type = "home";
  contact.email_addresses.push_back(std::move(home_email));
  EmailAddress work_email;
  work_email.value = "afrancois@work.example.com";
  work_email.type = "work";
  contact.email_addresses.push_back(std::move(work_email));
  Name name;
  name.family_name = "Francois";
  name.given_name = "Andre";
  contact.name = std::move(name);

  base::Value::Dict dict = std::move(contact).ToDict();

  EXPECT_THAT(dict, IsJson(R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
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
