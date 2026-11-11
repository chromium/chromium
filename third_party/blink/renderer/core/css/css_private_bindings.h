// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_BINDINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_BINDINGS_H_

#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// The @private names in scope at a point in a stylesheet: those declared for
// the enclosing style rule, plus those from the surrounding scope. Maps each
// authored name to the hygienic name its declarations are stored under.
// References stay authored after parsing, so cascade resolution uses these
// bindings to find their hygienic names.
class CSSPrivateBindings final : public GarbageCollected<CSSPrivateBindings> {
 public:
  CSSPrivateBindings(const CSSPrivateBindings* parent,
                     PrivateNameMap own_bindings)
      : parent_(parent), own_bindings_(std::move(own_bindings)) {}

  static bool IsPrivateName(const AtomicString& name) {
    return name.starts_with(PropertyRegistry::kPrivatePropertyNamePrefix);
  }

  // Returns the hygienic name for `name`, or null if it isn't private here.
  const AtomicString& Lookup(const AtomicString& name) const {
    auto it = own_bindings_.find(name);
    if (it != own_bindings_.end()) {
      return it->value;
    }
    return parent_ ? parent_->Lookup(name) : g_null_atom;
  }

  // Private initializers only see names declared for their enclosing style
  // rule.
  const CSSPrivateBindings* ForPrivateDeclarations() const {
    if (!parent_) {
      return this;
    }
    if (!private_declaration_scope_) {
      private_declaration_scope_ =
          MakeGarbageCollected<CSSPrivateBindings>(nullptr, own_bindings_);
    }
    return private_declaration_scope_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(parent_);
    visitor->Trace(private_declaration_scope_);
  }

 private:
  // The enclosing scope, or null for the outermost scope.
  Member<const CSSPrivateBindings> parent_;
  PrivateNameMap own_bindings_;
  mutable Member<const CSSPrivateBindings> private_declaration_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_BINDINGS_H_
