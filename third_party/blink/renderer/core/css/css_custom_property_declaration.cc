// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"

namespace blink {

void CSSCustomPropertyDeclaration::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(parser_context_);
  CSSValue::TraceAfterDispatch(visitor);
}

String CSSCustomPropertyDeclaration::CustomCSSText() const {
  DCHECK(value_);
  return value_->TokenRange().Serialize();
}

}  // namespace blink
