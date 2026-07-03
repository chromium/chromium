// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSPropertyRef::CSSPropertyRef(const AtomicString* name,
                               const Document& document)
    : property_id_(
          UnresolvedCSSPropertyID(document.GetExecutionContext(), *name)),
      custom_property_(property_id_ == CSSPropertyID::kVariable
                           ? CustomProperty(name, document)
                           : CustomProperty()) {}

CSSPropertyRef::CSSPropertyRef(const CSSPropertyName* name,
                               const Document& document)
    : property_id_(name->Id()),
      custom_property_(property_id_ == CSSPropertyID::kVariable
                           ? CustomProperty(&name->ToAtomicString(), document)
                           : CustomProperty()) {
  DCHECK_NE(name->Id(), CSSPropertyID::kInvalid);
}

CSSPropertyRef::CSSPropertyRef(const CSSProperty& property)
    : property_id_((property.PropertyID() == CSSPropertyID::kVariable &&
                    Variable::IsStaticInstance(property))
                       ? CSSPropertyID::kInvalid
                       : property.PropertyID()),
      custom_property_((property.PropertyID() == CSSPropertyID::kVariable &&
                        !Variable::IsStaticInstance(property))
                           ? static_cast<const CustomProperty&>(property)
                           : CustomProperty()) {}

}  // namespace blink
