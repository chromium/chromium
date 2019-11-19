// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const AtomicString& str)
    : CSSValue(kCustomIdentClass),
      string_(str),
      property_id_(CSSPropertyID::kInvalid) {}

CSSCustomIdentValue::CSSCustomIdentValue(CSSPropertyID id)
    : CSSValue(kCustomIdentClass), string_(), property_id_(id) {
  DCHECK(IsKnownPropertyID());
}

String CSSCustomIdentValue::CustomCSSText() const {
  if (IsKnownPropertyID()) {
    return CSSUnresolvedProperty::Get(property_id_)
        .GetPropertyNameAtomicString();
  }
  StringBuilder builder;
  SerializeIdentifier(string_, builder);
  return builder.ToString();
}

void CSSCustomIdentValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
