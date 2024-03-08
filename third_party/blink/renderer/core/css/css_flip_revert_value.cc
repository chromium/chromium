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
  builder.Append("-internal-revert-to(");

  const CSSProperty& property = CSSProperty::Get(property_id_);
  builder.Append(property.GetPropertyName());

  builder.Append(")");
  return builder.ReleaseString();
}

}  // namespace blink::cssvalue
