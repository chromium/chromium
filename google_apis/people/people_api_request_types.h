// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_

#include <string>

#include "base/values.h"

namespace google_apis::people {

// From the People API reference:
//
// A person's name. If the name is a mononym, the family name is empty.
//
// https://developers.google.com/people/api/rest/v1/people#name
struct Name {
  // The family name.
  std::string family_name;
  // The given name.
  std::string given_name;

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

// A contact-based Person sent to mutation endpoints. Unlike the general
// `Person` struct, this struct ensures that any fields which should be a
// singleton for contact-based Persons - biographies, birthdays, genders, names
// - are not repeated.
//
// Documentation for a generic Person:
// https://developers.google.com/people/api/rest/v1/people#resource:-person
struct Contact {
  // The person's name.
  //
  // As this is a contact-based Person, this is enforced to be a singleton.
  // However, this will still be serialised into an array under the `names` key
  // as that is expected from the API.
  // This will be not be serialised if it is empty.
  Name name;

  Contact();
  Contact(const Contact&);
  Contact& operator=(const Contact&);
  Contact(Contact&&);
  Contact& operator=(Contact&&);
  ~Contact();

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

}  // namespace google_apis::people

#endif  // GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_
