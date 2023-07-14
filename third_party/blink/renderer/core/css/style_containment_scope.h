// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/style/counter_directives.h"

namespace blink {

using CountersVector = HeapVector<Member<CounterNode>>;

// Represents the scope of the subtree that contains style.
// Manages quotes and child scopes.
// Managed by StyleContainmentScopeTree.
class CORE_EXPORT StyleContainmentScope final
    : public GarbageCollected<StyleContainmentScope> {
 public:
  explicit StyleContainmentScope(const Element* element)
      : element_(element), parent_(nullptr) {}

  // Handles the self remove.
  void ReattachToParent();

  void AttachQuote(LayoutQuote&);
  void DetachQuote(LayoutQuote&);
  void UpdateQuotes() const;

  void AttachCounter(CounterNode&);
  void DetachCounter(CounterNode&);
  void AttachLayoutCounter(LayoutCounter&);
  void DetachLayoutCounter(LayoutCounter&);
  void UpdateAllCounters(HashSet<AtomicString>&) const;

  void CreateCounterNodesForLayoutObject(LayoutObject&);
  void DeleteCounterNodesForLayoutObject(LayoutObject&,
                                         const ComputedStyle& style);

  bool IsAncestorOf(const Element*, const Element* stay_within = nullptr);

  void AppendChild(StyleContainmentScope*);
  void RemoveChild(StyleContainmentScope*);

  const Element* GetElement() { return element_; }
  StyleContainmentScope* Parent() { return parent_; }
  void SetParent(StyleContainmentScope* parent) { parent_ = parent; }
  const HeapVector<Member<LayoutQuote>>& Quotes() const { return quotes_; }
  const HeapHashMap<AtomicString, Member<CountersVector>>& Counters() const {
    return counters_;
  }
  const HeapVector<Member<StyleContainmentScope>>& Children() const {
    return children_;
  }

  void Trace(Visitor*) const;

 private:
  // Get the quote which would be the last in preorder traversal before we hit
  // Element*.
  const LayoutQuote* FindQuotePrecedingElement(const Element&) const;
  static const CounterNode* FindCounterPrecedingElement(const Element&,
                                                        const CountersVector&);
  int ComputeInitialQuoteDepth() const;
  int ComputeInitialCounterValue(const CountersVector&) const;
  CounterNode* GetCounterWithIdentifier(const LayoutObject&,
                                        const AtomicString&) const;
  bool UpdateCounters(CountersVector& counters) const;
  CounterNode* CreateCounter(LayoutObject&, const AtomicString&) const;

  // Element with style containment which is the root of the scope.
  Member<const Element> element_;
  // Parent scope.
  Member<StyleContainmentScope> parent_;
  // Vector of quotes.
  HeapVector<Member<LayoutQuote>> quotes_;
  // Map of counters.
  HeapHashMap<AtomicString, Member<CountersVector>> counters_;
  // Vector of children scope.
  HeapVector<Member<StyleContainmentScope>> children_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
