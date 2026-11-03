// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PARAM_VALUE_PAIR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PARAM_VALUE_PAIR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

// Represents a single param(<dashed-ident>: <declaration-value>?) function
// value for the link-parameters property.
//
// Spec: https://drafts.csswg.org/css-link-params/
class CORE_EXPORT CSSParamValuePair : public CSSValuePair {
 public:
  CSSParamValuePair(const CSSCustomIdentValue& name,
                    const CSSUnparsedDeclarationValue& value)
      : CSSValuePair(kParamValuePairClass, &name, &value) {}

  const CSSCustomIdentValue& Name() const {
    return To<CSSCustomIdentValue>(First());
  }
  const CSSUnparsedDeclarationValue& Value() const {
    return To<CSSUnparsedDeclarationValue>(Second());
  }

  String CustomCSSText() const;
  void TraceAfterDispatch(blink::Visitor* visitor) const {
    CSSValuePair::TraceAfterDispatch(visitor);
  }
};

template <>
struct DowncastTraits<CSSParamValuePair> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsParamValuePair();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PARAM_VALUE_PAIR_H_
