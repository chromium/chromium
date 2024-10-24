// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_request_types.h"

#include <utility>

#include "base/values.h"

namespace google_apis::people {

base::Value::Dict Name::ToDict() && {
  base::Value::Dict dict;

  if (!family_name.empty()) {
    dict.Set("familyName", std::move(family_name));
  }
  if (!given_name.empty()) {
    dict.Set("givenName", std::move(given_name));
  }

  return dict;
}

Contact::Contact() = default;
Contact::Contact(const Contact&) = default;
Contact& Contact::operator=(const Contact&) = default;
Contact::Contact(Contact&&) = default;
Contact& Contact::operator=(Contact&&) = default;
Contact::~Contact() = default;

base::Value::Dict Contact::ToDict() && {
  base::Value::Dict dict;

  if (base::Value::Dict name_dict = std::move(name).ToDict();
      !name_dict.empty()) {
    auto names = base::Value::List::with_capacity(1);
    names.Append(std::move(name_dict));
    dict.Set("names", std::move(names));
  }

  return dict;
}

}  // namespace google_apis::people
