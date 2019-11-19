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
  return html_label_element && html_label_element->control() == ownerNode();
}

}  // namespace blink
