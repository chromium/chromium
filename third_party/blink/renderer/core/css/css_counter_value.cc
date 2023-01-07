// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace cssvalue {

String CSSCounterValue::CustomCSSText() const {
  StringBuilder result;
  if (Separator().empty())
    result.Append("counter(");
  else
    result.Append("counters(");

  result.Append(Identifier());
  if (!Separator().empty()) {
    result.Append(", ");
    result.Append(separator_->CssText());
  }
  bool is_default_list_style = ListStyle() == "decimal";
  if (!is_default_list_style) {
    result.Append(", ");
    result.Append(list_style_->CssText());
  }
  result.Append(')');

  return result.ReleaseString();
}

void CSSCounterValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(identifier_);
  visitor->Trace(list_style_);
  visitor->Trace(separator_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue

}  // namespace blink
