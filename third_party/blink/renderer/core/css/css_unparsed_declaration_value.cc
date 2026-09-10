// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions_memory.h"

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

uint32_t CSSUnparsedDeclarationValue::CustomHash() const {
  return HashMemory32(data_->OriginalText().RawByteSpan());
}

}  // namespace blink
