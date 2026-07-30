// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_symbols_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

String CSSSymbolsValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("symbols(");

  // When the <symbols-type> is omitted from the symbols() function, it is
  // implied to be 'symbolic', so it is left out to ensure that we return the
  // shortest serialization.
  if (system_ != CSSValueID::kSymbolic) {
    result.Append(CSSIdentifierValue::Create(system_)->CssText());
    result.Append(' ');
  }
  result.Append(symbols_->CssText());
  result.Append(')');
  return result.ReleaseString();
}

void CSSSymbolsValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(symbols_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
