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

  // Returns the hygienic name for `name`, or null if it isn't private here.
  const AtomicString& Lookup(const AtomicString& name) const {
    auto it = own_bindings_.find(name);
    if (it != own_bindings_.end()) {
      return it->value;
    }
    return parent_ ? parent_->Lookup(name) : g_null_atom;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(parent_); }

 private:
  // The enclosing scope, or null for the outermost scope.
  Member<const CSSPrivateBindings> parent_;
  PrivateNameMap own_bindings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_BINDINGS_H_
