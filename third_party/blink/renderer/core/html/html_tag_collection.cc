/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/html/html_tag_collection.h"

namespace blink {

HTMLTagCollection::HTMLTagCollection(ContainerNode& root_node,
                                     const AtomicString& qualified_name)
    : TagCollection(root_node, kHTMLTagCollectionType, qualified_name),
      lowered_qualified_name_(qualified_name.LowerASCII()) {
  DCHECK(root_node.GetDocument().IsHTMLDocument());
}

HTMLTagCollection::HTMLTagCollection(ContainerNode& root_node,
                                     CollectionType type,
                                     const AtomicString& qualified_name)
    : HTMLTagCollection(root_node, qualified_name) {
  DCHECK_EQ(type, kHTMLTagCollectionType);
}

}  // namespace blink
