/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/live_node_list.h"

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

namespace {

class IsMatch {
  STACK_ALLOCATED();

 public:
  IsMatch(const LiveNodeList& list) : list_(&list) {}

  bool operator()(const Element& element) const {
    return list_->ElementMatches(element);
  }

 private:
  const LiveNodeList* list_;
};

}  // namespace

LiveNodeList::LiveNodeList(ContainerNode& owner_node,
                           CollectionType collection_type,
                           NodeListInvalidationType invalidation_type,
                           NodeListSearchRoot search_root)
    : LiveNodeListBase(owner_node,
                       search_root,
                       invalidation_type,
                       collection_type) {
  // Keep this in the child class because |registerNodeList| requires wrapper
  // tracing and potentially calls virtual methods which is not allowed in a
  // base class constructor.
  GetDocument().RegisterNodeList(this);
}

Node* LiveNodeList::VirtualOwnerNode() const {
  return &ownerNode();
}

void LiveNodeList::InvalidateCache(Document*) const {
  collection_items_cache_.Invalidate();
}

unsigned LiveNodeList::length() const {
  return collection_items_cache_.NodeCount(*this);
}

Element* LiveNodeList::item(unsigned offset) const {
  return collection_items_cache_.NodeAt(*this, offset);
}

Element* LiveNodeList::TraverseToFirst() const {
  return ElementTraversal::FirstWithin(RootNode(), IsMatch(*this));
}

Element* LiveNodeList::TraverseToLast() const {
  return ElementTraversal::LastWithin(RootNode(), IsMatch(*this));
}

Element* LiveNodeList::TraverseForwardToOffset(unsigned offset,
                                               Element& current_element,
                                               unsigned& current_offset) const {
  return TraverseMatchingElementsForwardToOffset(
      current_element, &RootNode(), offset, current_offset, IsMatch(*this));
}

Element* LiveNodeList::TraverseBackwardToOffset(
    unsigned offset,
    Element& current_element,
    unsigned& current_offset) const {
  return TraverseMatchingElementsBackwardToOffset(
      current_element, &RootNode(), offset, current_offset, IsMatch(*this));
}

void LiveNodeList::Trace(Visitor* visitor) const {
  visitor->Trace(collection_items_cache_);
  LiveNodeListBase::Trace(visitor);
  NodeList::Trace(visitor);
}

}  // namespace blink
