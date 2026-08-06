// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_content_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace cssvalue {

String CSSCounterContentValue::CustomCSSText() const {
  StringBuilder result;
  if (Separator().empty()) {
    result.Append("counter(");
  } else {
    result.Append("counters(");
  }

  SerializeIdentifier(Identifier(), result);
  if (!Separator().empty()) {
    result.Append(", ");
    result.Append(separator_->CssText());
  }
  // 'decimal' is the initial <counter-style> and is omitted when serializing.
  // A symbols() list style is never the default, so it is always serialized.
  bool is_default_list_style =
      !ListStyleIsSymbolsFunction() && ListStyleName() == "decimal";
  if (!is_default_list_style) {
    result.Append(", ");
    result.Append(list_style_->CssText());
  }
  result.Append(')');

  return result.ReleaseString();
}

const CSSCounterContentValue& CSSCounterContentValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  DCHECK(!IsScopedValue());
  // `EnsureScopedValue` is a no-op for a symbols() function, which is always
  // already scoped; only the identifier (and a name, when used) is populated.
  return *MakeGarbageCollected<CSSCounterContentValue>(
      &To<CSSCustomIdentValue>(identifier_->EnsureScopedValue(tree_scope)),
      &list_style_->EnsureScopedValue(tree_scope), separator_);
}

bool CSSCounterContentValue::HasRandomFunctions() const {
  return (identifier_ && identifier_->HasRandomFunctions()) ||
         (list_style_ && list_style_->HasRandomFunctions());
}

void CSSCounterContentValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(identifier_);
  visitor->Trace(list_style_);
  visitor->Trace(separator_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue

}  // namespace blink
