// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

#include "third_party/blink/renderer/core/css/property_registry.h"

namespace blink {

StyleInitialData::StyleInitialData(Document& document,
                                   const PropertyRegistry& registry) {
  for (const auto& entry : registry) {
    const CSSValue* specified_initial_value = entry.value->Initial();
    if (!specified_initial_value) {
      continue;
    }

    const CSSValue* computed_initial_value =
        &StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(
            document, *specified_initial_value);
    CSSVariableData* computed_initial_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            *computed_initial_value, false /* is_animation_tainted */);

    variables_.SetData(entry.key, computed_initial_data);
    variables_.SetValue(entry.key, computed_initial_value);
  }

  viewport_unit_flags_ = registry.GetViewportUnitFlags();
  document.AddViewportUnitFlags(viewport_unit_flags_);
}

bool StyleInitialData::operator==(const StyleInitialData& other) const {
  return variables_ == other.variables_;
}

}  // namespace blink
