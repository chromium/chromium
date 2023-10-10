// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSPropertyRef::CSSPropertyRef(const String& name, const Document& document)
    : property_id_(
          UnresolvedCSSPropertyID(document.GetExecutionContext(), name)) {
  if (property_id_ == CSSPropertyID::kVariable) {
    custom_property_ = CustomProperty(AtomicString(name), document);
  }
}

CSSPropertyRef::CSSPropertyRef(const CSSPropertyName& name,
                               const Document& document)
    : property_id_(name.Id()) {
  DCHECK_NE(name.Id(), CSSPropertyID::kInvalid);
  if (property_id_ == CSSPropertyID::kVariable) {
    custom_property_ = CustomProperty(name.ToAtomicString(), document);
  }
}

CSSPropertyRef::CSSPropertyRef(const CSSProperty& property)
    : property_id_(property.PropertyID()) {
  if (property.PropertyID() == CSSPropertyID::kVariable) {
    if (!Variable::IsStaticInstance(property)) {
      custom_property_ = static_cast<const CustomProperty&>(property);
    } else {
      property_id_ = CSSPropertyID::kInvalid;
    }
  }
}

}  // namespace blink
