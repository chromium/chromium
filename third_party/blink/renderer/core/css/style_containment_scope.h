// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"

namespace blink {

class LayoutCounter;
class CountersScope;
class CountersScopeTree;

// Represents the scope of the subtree that contains style.
// Manages quotes and child scopes.
// Managed by StyleContainmentScopeTree.
class StyleContainmentScope final
    : public GarbageCollected<StyleContainmentScope> {
 public:
  StyleContainmentScope(const Element* element,
                        StyleContainmentScopeTree* style_containment_tree);

  // Handles the self remove.
  void ReattachToParent();

  void AttachQuote(LayoutQuote&);
  void DetachQuote(LayoutQuote&);
  void UpdateQuotes() const;

  CORE_EXPORT CountersScope* FindCountersScopeForElement(
      const Element&,
      const AtomicString&) const;
  CORE_EXPORT void CreateCounterNodesForLayoutObject(LayoutObject&);
  CORE_EXPORT void CreateCounterNodeForLayoutCounter(LayoutCounter&);
  void CreateListItemCounterNodeForLayoutObject(LayoutObject&);
  void RemoveCounterNodeForLayoutCounter(LayoutCounter&);
  void ReparentCountersToStyleScope(StyleContainmentScope&);
  void UpdateCounters() const;

  bool IsAncestorOf(const Element*, const Element* stay_within = nullptr);

  void AppendChild(StyleContainmentScope*);
  void RemoveChild(StyleContainmentScope*);

  const Element* GetElement() { return element_; }
  CountersScopeTree* GetCountersScopeTree() { return counters_tree_; }
  StyleContainmentScope* Parent() { return parent_; }
  void SetParent(StyleContainmentScope* parent) { parent_ = parent; }
  const HeapVector<Member<LayoutQuote>>& Quotes() const { return quotes_; }
  const HeapVector<Member<StyleContainmentScope>>& Children() const {
    return children_;
  }

  StyleContainmentScopeTree* GetStyleContainmentScopeTree() const {
    return style_containment_tree_;
  }

  void Trace(Visitor*) const;

#if DCHECK_IS_ON()
  String ScopesTreeToString(wtf_size_t depth = 0u) const;
#endif  // DCHECK_IS_ON()

 private:
  // Get the quote which would be the last in preorder traversal before we hit
  // Element*.
  const LayoutQuote* FindQuotePrecedingElement(const Element&) const;
  int ComputeInitialQuoteDepth() const;

  // Element with style containment which is the root of the scope.
  Member<const Element> element_;
  // Parent scope.
  Member<StyleContainmentScope> parent_;
  // Vector of quotes.
  HeapVector<Member<LayoutQuote>> quotes_;
  // Counters tree.
  Member<CountersScopeTree> counters_tree_;
  // Vector of children scope.
  HeapVector<Member<StyleContainmentScope>> children_;
  // Style containment tree.
  WeakMember<StyleContainmentScopeTree> style_containment_tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
