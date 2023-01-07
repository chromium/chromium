/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/dom/tag_collection.h"

#include "third_party/blink/renderer/core/dom/node_rare_data.h"

namespace blink {

TagCollection::TagCollection(ContainerNode& root_node,
                             CollectionType type,
                             const AtomicString& qualified_name)
    : HTMLCollection(root_node, type, kDoesNotOverrideItemAfter),
      qualified_name_(qualified_name) {}

TagCollection::~TagCollection() = default;

bool TagCollection::ElementMatches(const Element& test_node) const {
  if (qualified_name_ == g_star_atom)
    return true;

  return qualified_name_ == test_node.TagQName().ToString();
}

TagCollectionNS::TagCollectionNS(ContainerNode& root_node,
                                 CollectionType type,
                                 const AtomicString& namespace_uri,
                                 const AtomicString& local_name)
    : HTMLCollection(root_node, type, kDoesNotOverrideItemAfter),
      namespace_uri_(namespace_uri),
      local_name_(local_name) {
  DCHECK(namespace_uri_.IsNull() || !namespace_uri_.empty());
}

TagCollectionNS::~TagCollectionNS() = default;

bool TagCollectionNS::ElementMatches(const Element& test_node) const {
  // Implements
  // https://dom.spec.whatwg.org/#concept-getelementsbytagnamens
  if (local_name_ != g_star_atom && local_name_ != test_node.localName())
    return false;

  return namespace_uri_ == g_star_atom ||
         namespace_uri_ == test_node.namespaceURI();
}

}  // namespace blink
