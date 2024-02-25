// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class Element;
class CounterNode;
class StyleContainmentScope;

using CountersVector = HeapVector<Member<CounterNode>>;

// Represents the scope of counters, the first counter represents the root.
// Managed by CountersScopeTree.
class CORE_EXPORT CountersScope final : public GarbageCollected<CountersScope> {
 public:
  void AttachCounter(CounterNode&);
  void DetachCounter(CounterNode&);
  void ClearCounters();
  CounterNode& FirstCounter() const;
  CountersVector& Counters() { return counters_; }
  static wtf_size_t FindCounterIndexPrecedingCounter(
      const CounterNode& search_counter,
      const CountersVector& counters);

  // is_dirty indicates that the values of counters should be updated.
  // It is cleared after the UpdateCounters.
  bool IsDirty() const { return is_dirty_; }
  void SetIsDirty(bool is_dirty) { is_dirty_ = is_dirty; }
  void UpdateCounters(const AtomicString& identifier, bool force_update = true);

  enum class SearchScope { SelfSearch, SelfAndAncestorSearch, AncestorSearch };
  // Finds the counter that precedes `counter`.
  // The search can be scoped to only this counter scope,
  // this + ancestors, only ancestors.
  // Also the search can proceed to ancestor style scopes.
  CounterNode* FindPreviousCounterFrom(CounterNode& counter,
                                       SearchScope search_scope,
                                       const AtomicString& identifier,
                                       bool leave_style_scope = true);

  void AppendChild(CountersScope&);
  void RemoveChild(CountersScope&);
  void ClearChildren();
  HeapVector<Member<CountersScope>>& Children() { return children_; }

  StyleContainmentScope* StyleScope() const { return scope_.Get(); }
  void SetStyleScope(StyleContainmentScope* scope) {
    scope_ = scope;
    is_dirty_ = true;
  }
  CountersScope* Parent() const { return parent_.Get(); }
  void SetParent(CountersScope* parent) {
    parent_ = parent;
    is_dirty_ = true;
  }

  Element& RootElement() const;
  Element& RootNonPseudoElement() const;

  void Trace(Visitor*) const;

 private:
  bool UpdateOwnCounters(bool force_update, const AtomicString& identifier);
  void UpdateChildCounters(const AtomicString& identifier, bool force_update);
  CounterNode* FindPreviousCounterWithinStyleScope(CounterNode& counter,
                                                   SearchScope search_scope);
  CounterNode* FindPreviousCounterInAncestorStyleScopes(
      CounterNode& counter,
      const AtomicString& identifier);

  bool is_dirty_;
  // Style containment scope.
  Member<StyleContainmentScope> scope_;
  // Parent counters scope.
  Member<CountersScope> parent_;
  // Vector of counters.
  CountersVector counters_;
  // Vector of children scope.
  HeapVector<Member<CountersScope>> children_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_H_
