/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_COMPUTED_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_COMPUTED_STYLE_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

inline const ComputedStyle* Node::GetComputedStyle() const {
  if (const auto* element = DynamicTo<Element>(this)) {
    return element->GetComputedStyle();
  }
  // Text nodes and Document.
  if (LayoutObject* layout_object = GetLayoutObject())
    return layout_object->Style();
  return nullptr;
}

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_COMPUTED_STYLE_H_
