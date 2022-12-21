// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

void CSSValuePair::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(first_);
  visitor->Trace(second_);
  CSSValue::TraceAfterDispatch(visitor);
}
}  // namespace blink
