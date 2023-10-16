// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_TREE_H_

#include "third_party/blink/renderer/core/css/counters_scope.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class LayoutObject;
class LayoutCounter;

using ScopesVector = HeapVector<Member<CountersScope>>;
using ScopesMap = HeapHashMap<AtomicString, Member<ScopesVector>>;

// Manages counters scopes. Lives inside the style containment scope.
class CORE_EXPORT CountersScopeTree final
    : public GarbageCollected<CountersScopeTree> {
 public:
  explicit CountersScopeTree(StyleContainmentScope* style_scope)
      : list_item_("list-item"), style_scope_(style_scope) {}
  CountersScopeTree(const CountersScopeTree&) = delete;
  CountersScopeTree& operator=(const CountersScopeTree&) = delete;

  // Find in which counters scope the element belongs to.
  CountersScope* FindScopeForElement(const Element&, const AtomicString&);

  void CreateCountersForLayoutObject(LayoutObject&);
  void CreateCounterForLayoutObject(LayoutObject&, const AtomicString&);
  void CreateCounterForLayoutCounter(LayoutCounter&);
  void CreateListItemCounterForLayoutObject(LayoutObject&);

  void RemoveCounterForLayoutCounter(LayoutCounter&);
  void RemoveCounterFromScope(CounterNode&,
                              CountersScope&,
                              const AtomicString&);

  void UpdateCounters();
  ScopesMap& Scopes() { return scopes_; }

  void ReparentCountersToStyleScope(StyleContainmentScope& new_parent);
  StyleContainmentScope* StyleScope() const { return style_scope_.Get(); }
  void SetStyleScope(StyleContainmentScope* style_scope) {
    style_scope_ = style_scope;
  }

  void Trace(Visitor*) const;

#if DCHECK_IS_ON()
  String ToString(wtf_size_t depth = 0u) const;
#endif  // DCHECK_IS_ON()

 private:
  void AttachCounter(CounterNode&, const AtomicString& identifier);
  void CreateScope(CounterNode&,
                   CountersScope* parent,
                   const AtomicString& identifier);
  void RemoveEmptyScope(CountersScope&, const AtomicString&);

  ScopesMap scopes_;
  const AtomicString list_item_;
  Member<StyleContainmentScope> style_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_SCOPE_TREE_H_
