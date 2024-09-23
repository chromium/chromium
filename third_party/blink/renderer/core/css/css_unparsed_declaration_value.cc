// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"

namespace blink {

void CSSUnparsedDeclarationValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(parser_context_);
  visitor->Trace(data_);
}

String CSSUnparsedDeclarationValue::CustomCSSText() const {
  // We may want to consider caching this value.
  return data_->Serialize();
}

}  // namespace blink
