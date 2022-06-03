/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/child_node_list.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"

namespace blink {

ChildNodeList::ChildNodeList(ContainerNode& parent) : parent_(parent) {}

Node* ChildNodeList::VirtualOwnerNode() const {
  return &OwnerNode();
}

ChildNodeList::~ChildNodeList() = default;

Node* ChildNodeList::item(unsigned index) const {
  return collection_index_cache_.NodeAt(*this, index);
}

void ChildNodeList::ChildrenChanged(
    const ContainerNode::ChildrenChange& change) {
  if (change.IsChildInsertion()) {
    collection_index_cache_.NodeInserted();
  } else if (change.IsChildRemoval()) {
    collection_index_cache_.NodeRemoved();
  } else {
    collection_index_cache_.Invalidate();
  }
}

Node* ChildNodeList::TraverseForwardToOffset(unsigned offset,
                                             Node& current_node,
                                             unsigned& current_offset) const {
  DCHECK_LT(current_offset, offset);
  DCHECK_EQ(OwnerNode().childNodes(), this);
  DCHECK_EQ(&OwnerNode(), current_node.parentNode());
  for (Node* next = current_node.nextSibling(); next;
       next = next->nextSibling()) {
    if (++current_offset == offset)
      return next;
  }
  return nullptr;
}

Node* ChildNodeList::TraverseBackwardToOffset(unsigned offset,
                                              Node& current_node,
                                              unsigned& current_offset) const {
  DCHECK_GT(current_offset, offset);
  DCHECK_EQ(OwnerNode().childNodes(), this);
  DCHECK_EQ(&OwnerNode(), current_node.parentNode());
  for (Node* previous = current_node.previousSibling(); previous;
       previous = previous->previousSibling()) {
    if (--current_offset == offset)
      return previous;
  }
  return nullptr;
}

void ChildNodeList::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(collection_index_cache_);
  NodeList::Trace(visitor);
}

}  // namespace blink
