// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/list_style_type_data.h"

#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

void ListStyleTypeData::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(counter_style_);
}

// static
ListStyleTypeData* ListStyleTypeData::CreateString(const AtomicString& value) {
  return MakeGarbageCollected<ListStyleTypeData>(Type::kString, value, nullptr);
}

// static
ListStyleTypeData* ListStyleTypeData::CreateCounterStyle(
    const AtomicString& name,
    const TreeScope* tree_scope) {
  return MakeGarbageCollected<ListStyleTypeData>(Type::kCounterStyle, name,
                                                 tree_scope);
}

bool ListStyleTypeData::IsCounterStyleReferenceValid(Document& document) const {
  if (!IsCounterStyle()) {
    DCHECK(!counter_style_);
    return true;
  }

  if (!counter_style_ || counter_style_->IsDirty()) {
    return false;
  }

  // Even if the referenced counter style is clean, it may still be stale if new
  // counter styles have been inserted, in which case the same (scope, name) now
  // refers to a different counter style. So we make an extra lookup to verify.
  return counter_style_ ==
         &document.GetStyleEngine().FindCounterStyleAcrossScopes(
             GetCounterStyleName(), GetTreeScope());
}

const CounterStyle& ListStyleTypeData::GetCounterStyle(
    Document& document) const {
  DCHECK(IsCounterStyle());
  if (!IsCounterStyleReferenceValid(document)) {
    counter_style_ = document.GetStyleEngine().FindCounterStyleAcrossScopes(
        GetCounterStyleName(), GetTreeScope());
  }
  return *counter_style_;
}

}  // namespace blink
