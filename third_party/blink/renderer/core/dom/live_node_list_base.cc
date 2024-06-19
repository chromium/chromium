/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/live_node_list_base.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/live_node_list.h"
#include "third_party/blink/renderer/core/html/html_collection.h"

namespace blink {

void LiveNodeListBase::InvalidateCacheForAttribute(
    const QualifiedName* attr_name) const {
  if (IsLiveNodeListType(GetType()))
    To<LiveNodeList>(this)->InvalidateCacheForAttribute(attr_name);
  else
    To<HTMLCollection>(this)->InvalidateCacheForAttribute(attr_name);
}

ContainerNode& LiveNodeListBase::RootNode() const {
  if (IsRootedAtTreeScope() && owner_node_->IsInTreeScope())
    return owner_node_->GetTreeScope().RootNode();
  return *owner_node_;
}

void LiveNodeListBase::DidMoveToDocument(Document& old_document,
                                         Document& new_document) {
  InvalidateCache(&old_document);
  old_document.UnregisterNodeList(this);
  new_document.RegisterNodeList(this);
}

}  // namespace blink
