// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_

#include "third_party/blink/renderer/core/css/ordered_scope_tree.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"

namespace blink {

// Explicit specializations of OrderedScope<LayoutQuote> static methods.
// These define how LayoutQuote items interact with contain:style scopes.
// Declared here, defined in style_containment_scope.cc.

template <>
const Element* OrderedScope<LayoutQuote>::GetItemElement(const LayoutQuote*);

template <>
void OrderedScope<LayoutQuote>::OnItemAttached(LayoutQuote*,
                                               OrderedScope<LayoutQuote>*);

template <>
void OrderedScope<LayoutQuote>::OnItemDetached(LayoutQuote*);

template <>
bool OrderedScope<LayoutQuote>::CreatesScope(const Element&);

template <>
void OrderedScope<LayoutQuote>::UpdateItemsInScope(
    const OrderedScope<LayoutQuote>*);

// Explicit template instantiation declarations.
// These prevent implicit instantiation in every translation unit that includes
// this header. The actual instantiation is in style_containment_scope.cc.
extern template class OrderedScope<LayoutQuote>;
extern template class OrderedScopeTree<LayoutQuote>;

// Type alias for backward compatibility.
using StyleContainmentScope = OrderedScope<LayoutQuote>;
using StyleContainmentScopeTree = OrderedScopeTree<LayoutQuote>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CONTAINMENT_SCOPE_H_
