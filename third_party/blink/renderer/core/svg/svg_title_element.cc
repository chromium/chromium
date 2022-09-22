/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_title_element.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SVGTitleElement::SVGTitleElement(Document& document)
    : SVGElement(svg_names::kTitleTag, document),
      ignore_title_updates_when_children_change_(false) {}

Node::InsertionNotificationRequest SVGTitleElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);
  if (!root_parent.isConnected())
    return kInsertionDone;
  if (HasChildren() && GetDocument().IsSVGDocument())
    GetDocument().SetTitleElement(this);
  return kInsertionDone;
}

void SVGTitleElement::RemovedFrom(ContainerNode& root_parent) {
  SVGElement::RemovedFrom(root_parent);
  if (root_parent.isConnected() && GetDocument().IsSVGDocument())
    GetDocument().RemoveTitle(this);
}

void SVGTitleElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);
  if (isConnected() && GetDocument().IsSVGDocument() &&
      !ignore_title_updates_when_children_change_)
    GetDocument().SetTitleElement(this);
}

void SVGTitleElement::SetText(const String& value) {
  ChildListMutationScope mutation(*this);

  {
    // Avoid calling Document::setTitleElement() during intermediate steps.
    base::AutoReset<bool> inhibit_title_update_scope(
        &ignore_title_updates_when_children_change_, !value.IsEmpty());
    RemoveChildren(kOmitSubtreeModifiedEvent);
  }

  if (!value.IsEmpty()) {
    AppendChild(GetDocument().createTextNode(value.Impl()),
                IGNORE_EXCEPTION_FOR_TESTING);
  }
}

}  // namespace blink
