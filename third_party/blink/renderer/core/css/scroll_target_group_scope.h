// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCROLL_TARGET_GROUP_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCROLL_TARGET_GROUP_SCOPE_H_

#include "third_party/blink/renderer/core/css/ordered_scope_tree.h"

namespace blink {

class Element;
class HTMLAnchorElement;

// OrderedScopeTree is defined in ordered_scope_tree.h.

// Explicit specializations of OrderedScope<HTMLAnchorElement> static methods.
// These define how HTMLAnchorElement items interact with scroll-target-group
// scopes. Elements with scroll-target-group: auto create scopes, and anchor
// elements with scroll targets participate in those scopes.
// Declared here, defined in scroll_target_group_scope.cc.

template <>
const Element* OrderedScope<HTMLAnchorElement>::GetItemElement(
    const HTMLAnchorElement*);

template <>
void OrderedScope<HTMLAnchorElement>::OnItemAttached(
    HTMLAnchorElement*,
    OrderedScope<HTMLAnchorElement>*);

template <>
void OrderedScope<HTMLAnchorElement>::OnItemDetached(HTMLAnchorElement*);

template <>
bool OrderedScope<HTMLAnchorElement>::CreatesScope(const Element&);

template <>
bool OrderedScope<HTMLAnchorElement>::StoresItemsInScope();

template <>
void OrderedScope<HTMLAnchorElement>::OnScopeCleared(
    OrderedScope<HTMLAnchorElement>*);

template <>
void OrderedScope<HTMLAnchorElement>::OnScopeReattachItems(
    OrderedScope<HTMLAnchorElement>*,
    OrderedScope<HTMLAnchorElement>*);

template <>
void OrderedScope<HTMLAnchorElement>::OnScopeMoveItemsFromParent(
    OrderedScope<HTMLAnchorElement>*,
    OrderedScope<HTMLAnchorElement>*);

template <>
void OrderedScope<HTMLAnchorElement>::UpdateItemsInScope(
    const OrderedScope<HTMLAnchorElement>*);

// Called when a new scroll-target-group scope is created.
// Collects anchor descendants that have valid scroll targets.
template <>
void OrderedScope<HTMLAnchorElement>::OnScopeCreated(
    OrderedScope<HTMLAnchorElement>* scope);

// Explicit template instantiation declarations.
// These prevent implicit instantiation in every translation unit that includes
// this header. The actual instantiation is in scroll_target_group_scope.cc.
extern template class OrderedScope<HTMLAnchorElement>;
extern template class OrderedScopeTree<HTMLAnchorElement>;

// Type aliases for convenience.
using ScrollTargetGroupScope = OrderedScope<HTMLAnchorElement>;
using ScrollTargetGroupScopeTree = OrderedScopeTree<HTMLAnchorElement>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCROLL_TARGET_GROUP_SCOPE_H_
