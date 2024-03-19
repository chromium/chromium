// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

// CSSFlipRevertValue should generally not be observable, but having
// serialization code is useful for debugging purposes (if nothing else).
String CSSFlipRevertValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append("-internal-flip-revert(");

  const CSSProperty& property = CSSProperty::Get(property_id_);
  builder.Append(property.GetPropertyName());

  // Note: `transform_` is not represented in the serialization, as there's currently
  //       no need for that. (The serialization is not web-exposed).

  builder.Append(")");
  return builder.ReleaseString();
}

}  // namespace blink::cssvalue
