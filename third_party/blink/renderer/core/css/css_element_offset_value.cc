// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_element_offset_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

CSSElementOffsetValue::CSSElementOffsetValue(const CSSValue* target,
                                             const CSSValue* edge,
                                             const CSSValue* threshold)
    : CSSValue(kElementOffsetClass),
      target_(target),
      edge_(edge),
      threshold_(threshold) {
  DCHECK(target && target->IsFunctionValue() &&
         To<CSSFunctionValue>(*target).FunctionType() == CSSValueID::kSelector);
  DCHECK(!edge || edge->IsIdentifierValue());
  DCHECK(!threshold || threshold->IsNumericLiteralValue());
}

String CSSElementOffsetValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(target_->CssText());
  if (edge_) {
    result.Append(' ');
    result.Append(edge_->CssText());
  }
  if (threshold_) {
    result.Append(' ');
    result.Append(threshold_->CssText());
  }
  return result.ToString();
}

bool CSSElementOffsetValue::Equals(const CSSElementOffsetValue& other) const {
  return DataEquivalent(target_, other.target_) &&
         DataEquivalent(edge_, other.edge_) &&
         DataEquivalent(threshold_, other.threshold_);
}

void CSSElementOffsetValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(target_);
  visitor->Trace(edge_);
  visitor->Trace(threshold_);
}

}  // namespace cssvalue
}  // namespace blink
