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

class ContainerQuery;

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

  // Since container queries cannot be evaluated at RuleSet creation time
  // (where we also create the MixinParameterBindings), we cannot always
  // create this fully. Thus, locals are split into two; first, there's
  // base_locals_, which are always active and the base. Then, there's the
  // (possibly empty) set of CQ-dependent locals that are applied on top
  // of base_locals_; for each property name, we have a series of values with
  // associated ContainerQuery to test, in declaration order.
  // This means we can start from the back, and stop as soon as we see
  // the first matching CQ.
  struct CQDependentValue {
    DISALLOW_NEW();

    Member<CSSVariableData> data;
    Member<ContainerQuery> container_query;

    void Trace(Visitor* visitor) const;
  };

  MixinParameterBindings(
      HeapHashMap<String, Binding> bindings,
      HeapHashMap<String, Member<CSSVariableData>> base_locals,
      HeapHashMap<String, HeapVector<CQDependentValue>>
          conditional_override_locals,
      const MixinParameterBindings* previous_in_env_chain)
      : bindings_(std::move(bindings)),
        base_locals_(std::move(base_locals)),
        conditional_override_locals_(std::move(conditional_override_locals)),
        parent_mixin_(previous_in_env_chain),
        hash_(ComputeHash()) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(parent_mixin_);
    visitor->Trace(bindings_);
    visitor->Trace(base_locals_);
    visitor->Trace(conditional_override_locals_);
  }

  // NOTE: Equality here is only used for the MPC, where false negatives
  // are OK. In particular, we compare bindings one level at a time;
  // if we have an entry for e.g. “--foo: bar;” and the other side
  // does not, we will return false even if a _parent_ of the other side
  // does. Doing anything else would rapidly get very complicated when
  // they can e.g. refer to each other with var().
  bool operator==(const MixinParameterBindings& other) const;

  const HeapHashMap<String, Binding>& GetBindings() const { return bindings_; }
  const HeapHashMap<String, Member<CSSVariableData>>& GetBaseLocals() const {
    return base_locals_;
  }
  const HeapHashMap<String, HeapVector<CQDependentValue>>&
  GetConditionalOverrideLocals() const {
    return conditional_override_locals_;
  }

  const MixinParameterBindings* GetParentMixin() const { return parent_mixin_; }

  // Returns a hash of all the bindings, mixed with the parents' hash.
  // (We don't hash the CSSSyntaxDefinition, so there may be false positives
  // in weird cases.) The same caveats as operator== apply.
  unsigned GetHash() const { return hash_; }

 private:
  unsigned ComputeHash() const;

  HeapHashMap<String, Binding> bindings_;
  HeapHashMap<String, Member<CSSVariableData>> base_locals_;
  HeapHashMap<String, HeapVector<CQDependentValue>>
      conditional_override_locals_;

  Member<const MixinParameterBindings> parent_mixin_;
  unsigned hash_;
};

template <>
struct VectorTraits<MixinParameterBindings::CQDependentValue>
    : VectorTraitsBase<MixinParameterBindings::CQDependentValue> {
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
  static const bool kCanTraceConcurrently = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_PARAMETER_BINDINGS_H_
