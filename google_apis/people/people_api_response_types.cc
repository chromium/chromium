// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_response_types.h"

#include "base/json/json_value_converter.h"

namespace google_apis::people {

void Person::RegisterJSONConverter(
    base::JSONValueConverter<Person>* converter) {
  converter->RegisterStringField("resourceName", &Person::resource_name);
}

}  // namespace google_apis::people
