// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

MathMLElement::MathMLElement(const QualifiedName& tagName,
                             Document& document,
                             ConstructionType constructionType)
    : Element(tagName, &document, constructionType) {}

MathMLElement::~MathMLElement() {}

static inline bool IsValidDirAttribute(const AtomicString& value) {
  return DeprecatedEqualIgnoringCase(value, "ltr") ||
         DeprecatedEqualIgnoringCase(value, "rtl");
}

// Keywords from MathML3 and CSS font-size are skipped.
static inline bool IsDisallowedMathSizeAttribute(const AtomicString& value) {
  return DeprecatedEqualIgnoringCase(value, "medium") ||
         value.EndsWith("large", kTextCaseASCIIInsensitive) ||
         value.EndsWith("small", kTextCaseASCIIInsensitive) ||
         DeprecatedEqualIgnoringCase(value, "smaller") ||
         DeprecatedEqualIgnoringCase(value, "larger");
}

bool MathMLElement::IsPresentationAttribute(const QualifiedName& name) const {
  // TODO(crbug.com/1023292, crbug.com/1023296): add support for display,
  // displaystyle and scriptlevel.
  if (name == html_names::kDirAttr || name == mathml_names::kMathsizeAttr ||
      name == mathml_names::kMathcolorAttr ||
      name == mathml_names::kMathbackgroundAttr)
    return true;
  return Element::IsPresentationAttribute(name);
}

void MathMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  // TODO(crbug.com/1023292, crbug.com/1023296): add support for display,
  // displaystyle and scriptlevel.
  if (name == html_names::kDirAttr) {
    if (IsValidDirAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDirection,
                                              value);
    }
  } else if (name == mathml_names::kMathsizeAttr) {
    if (!IsDisallowedMathSizeAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFontSize,
                                              value);
    }
  } else if (name == mathml_names::kMathbackgroundAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == mathml_names::kMathcolorAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kColor,
                                            value);
  } else {
    Element::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void MathMLElement::ParseAttribute(const AttributeModificationParams& param) {
  const AtomicString& event_name =
      HTMLElement::EventNameForAttributeName(param.name);
  if (!event_name.IsNull()) {
    SetAttributeEventListener(
        event_name,
        CreateAttributeEventListener(this, param.name, param.new_value));
    return;
  }

  Element::ParseAttribute(param);
}

}  // namespace blink
