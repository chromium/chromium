// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_initial_data.h"

#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

StyleInitialData::StyleInitialData(const PropertyRegistry& registry) {
  for (const auto& entry : registry) {
    CSSVariableData* data = entry.value->InitialVariableData();
    if (!data)
      continue;
    variables_.SetData(entry.key, data);
    if (const CSSValue* initial = entry.value->Initial())
      variables_.SetValue(entry.key, initial);
  }
}

bool StyleInitialData::operator==(const StyleInitialData& other) const {
  return variables_ == other.variables_;
}

}  // namespace blink
