/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_title_element.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

HTMLTitleElement::HTMLTitleElement(Document& document)
    : HTMLElement(html_names::kTitleTag, document),
      ignore_title_updates_when_children_change_(false) {}

Node::InsertionNotificationRequest HTMLTitleElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (IsInDocumentTree())
    GetDocument().SetTitleElement(this);
  return kInsertionDone;
}

void HTMLTitleElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (insertion_point.IsInDocumentTree())
    GetDocument().RemoveTitle(this);
}

void HTMLTitleElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (IsInDocumentTree() && !ignore_title_updates_when_children_change_)
    GetDocument().SetTitleElement(this);
}

String HTMLTitleElement::text() const {
  StringBuilder result;

  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (auto* text_node = DynamicTo<Text>(n))
      result.Append(text_node->data());
  }

  return result.ToString();
}

void HTMLTitleElement::setText(const String& value) {
  ChildListMutationScope mutation(*this);

  {
    // Avoid calling Document::setTitleElement() during intermediate steps.
    base::AutoReset<bool> inhibit_title_update_scope(
        &ignore_title_updates_when_children_change_, !value.empty());
    RemoveChildren(kOmitSubtreeModifiedEvent);
  }

  if (!value.empty()) {
    AppendChild(GetDocument().createTextNode(value.Impl()),
                IGNORE_EXCEPTION_FOR_TESTING);
  }
}

}  // namespace blink
