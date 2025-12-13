// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/naming_scope.h"

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/style/style_name_scope.h"

namespace blink {

namespace {

bool IsWithinScope(const ScopedCSSName& lookup_name,
                   const StyleNameScope& scope) {
  if (scope.IsNone()) {
    return false;
  }
  if (scope.IsAll()) {
    return scope.AllTreeScope() == lookup_name.GetTreeScope();
  }
  const ScopedCSSNameList* scoped_names = scope.Names();
  CHECK(scoped_names);
  for (const Member<const ScopedCSSName>& scoped_name :
       scoped_names->GetNames()) {
    if (*scoped_name == lookup_name) {
      return true;
    }
  }
  return false;
}

}  // namespace

void NamingScope::Trace(Visitor* visitor) const {
  visitor->Trace(name_);
  visitor->Trace(scope_element_);
}

// static
const Element* NamingScope::FindScopeElement(
    const ScopedCSSName& name,
    const Element& start_element,
    base::FunctionRef<StyleNameScope(const ComputedStyle& style)> get_scope) {
  for (const Element* element = &start_element; element;
       element = LayoutTreeBuilderTraversal::ParentElement(*element)) {
    const ComputedStyle* style = element->GetComputedStyle();
    if (!style) {
      // TODO(crbug.com/384523570): We should not be here without a style,
      // but apparently that can happen [1]. This is likely related to poking
      // into a dirty layout tree during scroll snapshotting,
      // since UpdateSnapshot() is on the stack [1].
      //
      // [1] crbug.com/393395576
      continue;
    }

    const StyleNameScope& scope = get_scope(*style);
    if (IsWithinScope(name, scope)) {
      return element;
    }
  }
  return nullptr;
}

}  // namespace blink
