/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_RESOLVE_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_RESOLVE_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class Element;
class ComputedStyle;

// Wraps an Element for use by ElementRuleCollector. Computes various values
// from the element for quick access during style calculation. It is immutable.
class CORE_EXPORT ElementResolveContext {
  STACK_ALLOCATED();

 public:
  explicit ElementResolveContext(Element&);

  Element& GetElement() const { return *element_; }
  const Element* ParentElement() const { return parent_element_; }
  const Element* LayoutParentElement() const { return layout_parent_; }
  const ComputedStyle* RootElementStyle() const { return root_element_style_; }
  const ComputedStyle* ParentStyle() const {
    return ParentElement() ? ParentElement()->GetComputedStyle() : nullptr;
  }
  const ComputedStyle* LayoutParentStyle() const {
    return LayoutParentElement() ? LayoutParentElement()->GetComputedStyle()
                                 : nullptr;
  }
  EInsideLink ElementLinkState() const { return element_link_state_; }

 private:
  Element* element_;
  Element* parent_element_{nullptr};
  Element* layout_parent_{nullptr};
  const ComputedStyle* root_element_style_{nullptr};
  EInsideLink element_link_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_RESOLVE_CONTEXT_H_
