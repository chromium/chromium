// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"

namespace blink::cssvalue {

WTF::String CSSScopedKeywordValue::CustomCSSText() const {
  return AtomicString(getValueName(value_id_));
}

const CSSScopedKeywordValue& CSSScopedKeywordValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  DCHECK(needs_tree_scope_population_);
  CSSScopedKeywordValue* populated =
      MakeGarbageCollected<CSSScopedKeywordValue>(*this);
  populated->tree_scope_ = tree_scope;
  populated->needs_tree_scope_population_ = false;
  return *populated;
}

void CSSScopedKeywordValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
