// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"

namespace blink {
namespace cssvalue {

void CSSPendingSubstitutionValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(shorthand_value_);
}

String CSSPendingSubstitutionValue::CustomCSSText() const {
  return "";
}

}  // namespace cssvalue
}  // namespace blink
