// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_PARAMETER_BINDINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_PARAMETER_BINDINGS_H_

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// A set of custom mixin bindings at some given point in the stylesheet,
// i.e., which variable has which value (and which type is it supposed
// to match; we cannot check this when binding, so it needs to happen
// when substituting). Created when we @apply a mixin; StyleRules and
// other interested parties can point to a MixinParameterBindings,
// which contains its own bindings and then point backwards to the
// next set of upper bindings (if any), and so on in a linked list.
//
// This will be converted to function context at the time of application.
class MixinParameterBindings : public GarbageCollected<MixinParameterBindings> {
 public:
  struct Binding {
    DISALLOW_NEW();

    Member<CSSVariableData> value;
    Member<CSSVariableData> default_value;
    CSSSyntaxDefinition syntax;

    bool operator==(const Binding& other) const {
      return base::ValuesEquivalent(value, other.value) &&
             base::ValuesEquivalent(default_value, other.default_value) &&
             syntax == other.syntax;
    }

    void Trace(Visitor* visitor) const {
      visitor->Trace(value);
      visitor->Trace(default_value);
    }
  };

  MixinParameterBindings(HeapHashMap<String, Binding> bindings,
                         const MixinParameterBindings* previous_in_env_chain)
      : bindings_(bindings),
        parent_mixin_(previous_in_env_chain),
        hash_(ComputeHash()) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(parent_mixin_);
    visitor->Trace(bindings_);
  }

  // NOTE: Equality here is only used for the MPC, where false negatives
  // are OK. In particular, we compare bindings one level at a time;
  // if we have an entry for e.g. “--foo: bar;” and the other side
  // does not, we will return false even if a _parent_ of the other side
  // does. Doing anything else would rapidly get very complicated when
  // they can e.g. refer to each other with var().
  bool operator==(const MixinParameterBindings& other) const;

  const HeapHashMap<String, Binding>& GetBindings() const { return bindings_; }

  const MixinParameterBindings* GetParentMixin() const { return parent_mixin_; }

  // Returns a hash of all the bindings, mixed with the parents' hash.
  // (We don't hash the CSSSyntaxDefinition, so there may be false positives
  // in weird cases.) The same caveats as operator== apply.
  unsigned GetHash() const { return hash_; }

 private:
  unsigned ComputeHash() const;

  HeapHashMap<String, Binding> bindings_;
  Member<const MixinParameterBindings> parent_mixin_;
  unsigned hash_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_PARAMETER_BINDINGS_H_
