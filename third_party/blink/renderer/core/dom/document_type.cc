/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/document_type.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

DocumentType::DocumentType(Document* document,
                           const String& name,
                           const String& public_id,
                           const String& system_id)
    : Node(document, kCreateDocumentType),
      name_(name),
      public_id_(public_id),
      system_id_(system_id) {}

String DocumentType::nodeName() const {
  return name();
}

Node* DocumentType::Clone(Document& factory,
                          NodeCloningData&,
                          ContainerNode* append_to,
                          ExceptionState& append_exception_state) const {
  DocumentType* clone = MakeGarbageCollected<DocumentType>(
      &factory, name_, public_id_, system_id_);
  if (append_to) {
    append_to->AppendChild(clone, append_exception_state);
  }
  return clone;
}

Node::InsertionNotificationRequest DocumentType::InsertedInto(
    ContainerNode& insertion_point) {
  Node::InsertedInto(insertion_point);

  // DocumentType can only be inserted into a Document.
  DCHECK(parentNode()->IsDocumentNode());

  GetDocument().SetDoctype(this);

  return kInsertionDone;
}

void DocumentType::RemovedFrom(ContainerNode& insertion_point) {
  GetDocument().SetDoctype(nullptr);
  Node::RemovedFrom(insertion_point);
}

}  // namespace blink
