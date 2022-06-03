// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"

namespace blink {

void CSSVariableReferenceValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(parser_context_);
}

String CSSVariableReferenceValue::CustomCSSText() const {
  // We may want to consider caching this value.
  return data_->TokenRange().Serialize();
}

}  // namespace blink
