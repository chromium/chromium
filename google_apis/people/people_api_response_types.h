// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_PEOPLE_PEOPLE_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_PEOPLE_PEOPLE_API_RESPONSE_TYPES_H_

#include <string>

#include "base/json/json_value_converter.h"

namespace google_apis::people {

// A generic Person returned from the People API.
// While most Persons returned from the API are contact-based
// (https://developers.google.com/people#understanding_merged_person_data), this
// struct intentionally does not enforce any of the contact-based Person
// invariants - see the `Contact` struct for more details - and is intended to
// be a thin wrapper around the exact response returned from the API.
//
// From the People API reference:
//
// Information about a person merged from various data sources such as the
// authenticated user's contacts and profile data.
//
// Most fields can have multiple items. The items in a field have no guaranteed
// order, but each non-empty field is guaranteed to have exactly one field with
// `metadata.primary` set to true.
//
// https://developers.google.com/people/api/rest/v1/people#resource:-person
struct Person {
  // The resource name for the person, assigned by the server. An ASCII string
  // in the form of `people/{person_id}`.
  std::string resource_name;

  static void RegisterJSONConverter(
      base::JSONValueConverter<Person>* converter);
};

}  // namespace google_apis::people

#endif  // GOOGLE_APIS_PEOPLE_PEOPLE_API_RESPONSE_TYPES_H_
