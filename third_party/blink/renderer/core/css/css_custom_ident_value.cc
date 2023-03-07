// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const AtomicString& str)
    : CSSValue(kCustomIdentClass),
      string_(str),
      property_id_(CSSPropertyID::kInvalid) {
  needs_tree_scope_population_ = true;
}

CSSCustomIdentValue::CSSCustomIdentValue(CSSPropertyID id)
    : CSSValue(kCustomIdentClass), string_(), property_id_(id) {
  DCHECK(IsKnownPropertyID());
}

CSSCustomIdentValue::CSSCustomIdentValue(const ScopedCSSName& name)
    : CSSCustomIdentValue(name.GetName()) {
  tree_scope_ = name.GetTreeScope();
  needs_tree_scope_population_ = false;
}

String CSSCustomIdentValue::CustomCSSText() const {
  if (IsKnownPropertyID()) {
    return CSSUnresolvedProperty::Get(property_id_)
        .GetPropertyNameAtomicString();
  }
  StringBuilder builder;
  SerializeIdentifier(string_, builder);
  return builder.ReleaseString();
}

const CSSCustomIdentValue& CSSCustomIdentValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  DCHECK(this->needs_tree_scope_population_);
  CSSCustomIdentValue* populated =
      MakeGarbageCollected<CSSCustomIdentValue>(*this);
  populated->tree_scope_ = tree_scope;
  populated->needs_tree_scope_population_ = false;
  return *populated;
}

void CSSCustomIdentValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
