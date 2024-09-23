/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

LabelsNodeList::LabelsNodeList(ContainerNode& owner_node)
    : LiveNodeList(owner_node,
                   kLabelsNodeListType,
                   kInvalidateForFormControls,
                   NodeListSearchRoot::kTreeScope) {}

LabelsNodeList::LabelsNodeList(ContainerNode& owner_node, CollectionType type)
    : LabelsNodeList(owner_node) {
  DCHECK_EQ(type, kLabelsNodeListType);
}

LabelsNodeList::~LabelsNodeList() = default;

bool LabelsNodeList::ElementMatches(const Element& element) const {
  auto* html_label_element = DynamicTo<HTMLLabelElement>(element);
  return html_label_element && html_label_element->Control() == ownerNode();
}

ContainerNode& LabelsNodeList::RootNode() const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()) {
    return LiveNodeList::RootNode();
  }

  if (!ownerNode().IsInTreeScope()) {
    return ownerNode();
  }

  ContainerNode* root = &ownerNode().GetTreeScope().RootNode();

  // If the owner node is in a shadow tree and is the reference target of its
  // shadow host, traverse up to include the host's containing tree scope.
  Element* host = ownerNode().OwnerShadowHost();
  while (host &&
         host->GetShadowReferenceTarget(html_names::kForAttr) == &ownerNode()) {
    DCHECK(host->IsShadowIncludingAncestorOf(ownerNode()));
    root = &host->GetTreeScope().RootNode();
    host = host->OwnerShadowHost();
  }

  return *root;
}

Element* LabelsNodeList::Next(Element& current) const {
  if (current.GetShadowReferenceTarget(html_names::kForAttr) == &ownerNode()) {
    // If the owner node is the reference target of the current element,
    // drill into its shadow tree to continue iterating.
    DCHECK(current.IsShadowIncludingAncestorOf(ownerNode()));
    if (Element* first = ElementTraversal::FirstWithin(
            current.GetShadowRoot()->RootNode())) {
      return first;
    }
  }

  if (Element* next = ElementTraversal::Next(current)) {
    return next;
  }

  // If we've reached the end of the current shadow tree, move up to continue
  // traversing the rest of the host tree if the owner node is the host's
  // reference target.
  Element* host = current.OwnerShadowHost();
  while (host &&
         host->GetShadowReferenceTarget(html_names::kForAttr) == &ownerNode()) {
    DCHECK(host->IsShadowIncludingAncestorOf(ownerNode()));
    if (Element* next = ElementTraversal::Next(*host)) {
      return next;
    }
    host = host->OwnerShadowHost();
  }

  return nullptr;
}

Element* LabelsNodeList::Previous(Element& current) const {
  Element* prev = ElementTraversal::Previous(current);

  if (!prev) {
    // If we've reached the start of the current shadow tree, move up to
    // continue traversing the rest of the host tree if the owner node is the
    // host's reference target.
    Element* host = current.OwnerShadowHost();
    if (host &&
        host->GetShadowReferenceTarget(html_names::kForAttr) == &ownerNode()) {
      DCHECK(host->IsShadowIncludingAncestorOf(ownerNode()));
      return host;
    }
    return nullptr;
  } else if (prev->GetShadowReferenceTarget(html_names::kForAttr) ==
             &ownerNode()) {
    DCHECK(prev->IsShadowIncludingAncestorOf(ownerNode()));
    // If the owner node is the reference target of the previous element,
    // drill into its shadow tree to continue iterating.
    if (Element* last =
            ElementTraversal::LastWithin(prev->GetShadowRoot()->RootNode())) {
      return last;
    }
  }

  return prev;
}

Element* LabelsNodeList::TraverseToFirst() const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()) {
    return LiveNodeList::TraverseToFirst();
  }

  for (Element* ele = ElementTraversal::FirstWithin(RootNode()); ele;
       ele = Next(*ele)) {
    if (ElementMatches(*ele)) {
      return ele;
    }
  }

  return nullptr;
}

Element* LabelsNodeList::TraverseToLast() const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()) {
    return LiveNodeList::TraverseToLast();
  }

  for (Element* ele = ElementTraversal::LastWithin(RootNode()); ele;
       ele = Previous(*ele)) {
    if (ElementMatches(*ele)) {
      return ele;
    }
  }

  return nullptr;
}

Element* LabelsNodeList::TraverseForwardToOffset(
    unsigned offset,
    Element& current_node,
    unsigned& current_offset) const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()) {
    return LiveNodeList::TraverseForwardToOffset(offset, current_node,
                                                 current_offset);
  }

  for (Element* ele = Next(current_node); ele; ele = Next(*ele)) {
    if (ElementMatches(*ele)) {
      if (++current_offset == offset) {
        return ele;
      }
    }
  }

  return nullptr;
}

Element* LabelsNodeList::TraverseBackwardToOffset(
    unsigned offset,
    Element& current_node,
    unsigned& current_offset) const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()) {
    return LiveNodeList::TraverseBackwardToOffset(offset, current_node,
                                                  current_offset);
  }

  for (Element* ele = Previous(current_node); ele; ele = Previous(*ele)) {
    if (ElementMatches(*ele)) {
      if (--current_offset == offset) {
        return ele;
      }
    }
  }

  return nullptr;
}

}  // namespace blink
