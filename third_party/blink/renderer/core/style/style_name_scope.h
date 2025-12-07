// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NAME_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NAME_SCOPE_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents the computed value of properties which restrict names to relevant
// scopes such as:
// - 'anchor-scope': https://drafts.csswg.org/css-anchor-position/#anchor-scope
// - 'trigger-scope': https://drafts.csswg.org/css-animations-2/#trigger-scope
class CORE_EXPORT StyleNameScope {
  DISALLOW_NEW();

 public:
  enum class Type { kNone, kAll, kNames };

  StyleNameScope() = default;
  StyleNameScope(Type type,
                 const TreeScope* all_tree_scope,
                 const ScopedCSSNameList* names)
      : type_(type), all_tree_scope_(all_tree_scope), names_(names) {}

  bool operator==(const StyleNameScope& o) const {
    return type_ == o.type_ &&
           base::ValuesEquivalent(all_tree_scope_, o.all_tree_scope_) &&
           base::ValuesEquivalent(names_, o.names_);
  }

  bool IsNone() const { return type_ == Type::kNone; }
  bool IsAll() const { return type_ == Type::kAll; }
  const TreeScope* AllTreeScope() const { return all_tree_scope_.Get(); }
  const ScopedCSSNameList* Names() const { return names_.Get(); }

  void Trace(Visitor* visitor) const {
    visitor->Trace(all_tree_scope_);
    visitor->Trace(names_);
  }

 private:
  Type type_ = Type::kNone;
  // For Type::kAll.
  Member<const TreeScope> all_tree_scope_;
  // For Type::kNames.
  Member<const ScopedCSSNameList> names_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NAME_SCOPE_H_
