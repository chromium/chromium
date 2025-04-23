// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scope.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void AnchorScopedName::Trace(Visitor* visitor) const {
  visitor->Trace(name_);
  visitor->Trace(anchor_scope_element_);
}

namespace {

bool IsScopedByElement(const ScopedCSSName& lookup_name,
                       const Element& element) {
  const ComputedStyle* style = element.GetComputedStyle();
  if (!style) {
    // TODO(crbug.com/384523570): We should not be here without a style,
    // but apparently that can happen [1]. This is likely related to poking
    // into a dirty layout tree during scroll snapshotting,
    // since ValidateSnapshot() is on the stack [1].
    //
    // [1] crbug.com/393395576
    return false;
  }
  const StyleAnchorScope& anchor_scope = style->AnchorScope();
  if (anchor_scope.IsNone()) {
    return false;
  }
  if (anchor_scope.IsAll()) {
    return anchor_scope.AllTreeScope() == lookup_name.GetTreeScope();
  }
  const ScopedCSSNameList* scoped_names = anchor_scope.Names();
  CHECK(scoped_names);
  for (const Member<const ScopedCSSName>& scoped_name :
       scoped_names->GetNames()) {
    if (*scoped_name == lookup_name) {
      return true;
    }
  }
  return false;
}

const Element* AnchorScopeElement(const ScopedCSSName& name,
                                  const LayoutObject& layout_object) {
  for (const Element* element = To<Element>(layout_object.GetNode()); element;
       element = LayoutTreeBuilderTraversal::ParentElement(*element)) {
    if (IsScopedByElement(name, *element)) {
      return element;
    }
  }
  return nullptr;
}

}  // namespace

AnchorScopedName* ToAnchorScopedName(const ScopedCSSName& name,
                                     const LayoutObject& layout_object) {
  return MakeGarbageCollected<AnchorScopedName>(
      name, AnchorScopeElement(name, layout_object));
}

}  // namespace blink
