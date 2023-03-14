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
  // We want to use Serialize() and not TokenRange().Serialize(),
  // since we want to use the original text if possible:
  //
  // https://drafts.csswg.org/css-variables/#serializing-custom-props
  // “Specified values of custom properties must be serialized _exactly as
  // specified by the author_. Simplifications that might occur in other
  // properties, such as dropping comments, normalizing whitespace,
  // reserializing numeric tokens from their value, etc., must not occur.”
  DCHECK(value_);
  return value_->Serialize();
}

}  // namespace blink
