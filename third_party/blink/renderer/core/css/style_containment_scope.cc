// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_containment_scope.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"

namespace blink {

// Explicit specializations of OrderedScope<LayoutQuote> static methods.

template <>
const Element* OrderedScope<LayoutQuote>::GetItemElement(
    const LayoutQuote* quote) {
  return quote->GetOwningPseudo();
}

template <>
void OrderedScope<LayoutQuote>::OnItemAttached(
    LayoutQuote* quote,
    OrderedScope<LayoutQuote>* scope) {
  quote->SetScope(scope);
}

template <>
void OrderedScope<LayoutQuote>::OnItemDetached(LayoutQuote* quote) {
  quote->SetScope(nullptr);
}

template <>
bool OrderedScope<LayoutQuote>::CreatesScope(const Element& element) {
  return element.GetComputedStyle() &&
         element.ComputedStyleRef().ContainsStyle();
}

namespace {

// Computes the initial quote depth for a scope based on preceding quotes
// in ancestor scopes.
int ComputeInitialQuoteDepth(const StyleContainmentScope* scope) {
  if (scope->Items().empty()) {
    return 0;
  }

  // Find the first item's element to search for preceding quotes.
  const Element* first_item_element =
      StyleContainmentScope::GetItemElement(scope->Items().front());
  if (!first_item_element) {
    return 0;
  }

  // Search ancestor scopes for the quote that precedes the first item.
  for (const StyleContainmentScope* parent = scope->Parent(); parent;
       parent = parent->Parent()) {
    const LayoutQuote* preceding =
        parent->FindItemPrecedingElement(*first_item_element);
    if (preceding) {
      return preceding->GetNextDepth();
    }
  }
  return 0;
}

void UpdateQuotesRecursively(const StyleContainmentScope* scope) {
  int depth = ComputeInitialQuoteDepth(scope);

  for (LayoutQuote* quote : scope->Items()) {
    quote->SetDepth(depth);
    quote->UpdateText();
    depth = quote->GetNextDepth();
  }

  for (StyleContainmentScope* child : scope->Children()) {
    UpdateQuotesRecursively(child);
  }
}

}  // namespace

template <>
void OrderedScope<LayoutQuote>::UpdateItemsInScope(
    const OrderedScope<LayoutQuote>* scope) {
  UpdateQuotesRecursively(scope);
}

// Explicit template instantiation definitions.
// These provide the actual template implementations for LayoutQuote
// instantiations, preventing code duplication across translation units.
template class OrderedScope<LayoutQuote>;
template class OrderedScopeTree<LayoutQuote>;

}  // namespace blink
