// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_frame.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

void StyleScopeActivation::Trace(blink::Visitor* visitor) const {
  visitor->Trace(root);
}

void StyleScopeActivations::Trace(blink::Visitor* visitor) const {
  visitor->Trace(vector);
}

StyleScopeFrame* StyleScopeFrame::GetParentFrameOrNull(
    Element& parent_element) {
  if (parent_ && (&parent_->element_ == &parent_element)) {
    return parent_;
  }
  return nullptr;
}

StyleScopeFrame& StyleScopeFrame::GetParentFrameOrThis(
    Element& parent_element) {
  StyleScopeFrame* parent_frame = GetParentFrameOrNull(parent_element);
  return parent_frame ? *parent_frame : *this;
}

bool StyleScopeFrame::HasSeenImplicitScope(const StyleScope& style_scope) {
  if (!seen_implicit_scopes_) {
    seen_implicit_scopes_ = CalculateSeenImplicitScopes();
  }
  return seen_implicit_scopes_->Contains(&style_scope);
}

StyleScopeFrame::ScopeSet* StyleScopeFrame::CalculateSeenImplicitScopes() {
  bool owns_set;
  ScopeSet* scopes;

  auto add_triggered_scopes = [&owns_set, &scopes](Element& element) {
    if (const StyleScopeData* style_scope_data = element.GetStyleScopeData()) {
      for (const Member<const StyleScope>& style_scope :
           style_scope_data->GetTriggeredScopes()) {
        if (!owns_set) {
          // Copy-on-write.
          scopes = MakeGarbageCollected<ScopeSet>(*scopes);
          owns_set = true;
        }
        scopes->insert(style_scope);
      }
    }
  };

  Element* parent_element = element_.ParentOrShadowHostElement();
  StyleScopeFrame* parent_frame =
      parent_element ? StyleScopeFrame::GetParentFrameOrNull(*parent_element)
                     : nullptr;
  if (parent_frame) {
    // We've seen all scopes that the parent has seen ...
    owns_set = false;
    scopes = parent_frame->CalculateSeenImplicitScopes();
    // ... plus any new scopes seen on this element.
    add_triggered_scopes(element_);
  } else {
    // Add scopes for the whole ancestor chain. Note that we don't necessarily
    // have a StyleScopeFrame instance on the stack for the whole chain,
    // because style recalc can begin in the middle of the tree
    // (see StyleRecalcRoot).
    owns_set = true;
    scopes = MakeGarbageCollected<ScopeSet>();
    for (Element* e = &element_; e; e = e->ParentOrShadowHostElement()) {
      add_triggered_scopes(*e);
    }
  }

  return scopes;
}

}  // namespace blink
