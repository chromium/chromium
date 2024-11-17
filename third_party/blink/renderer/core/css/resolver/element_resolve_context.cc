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

#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"

#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

namespace blink {

// Builds pseudo element ancestors for rule matching:
// - For regular elements just returns empty array.
// - For pseudo elements (including nested pseudo elements) returns
// array of every pseudo element ancestor, including
// pseudo element for which rule matching is performed.
// This array is later used to check rules by simultaneously going
// through the array and rules sub selectors.
// E.g.: <li> element with ::after and ::marker inside that ::after:
// -- the rule li::after::marker, the array would be [after, marker],
// matching starts with originating element <li>, so the rule will be matched
// as <li> - li, after (from array) - ::after, marker (from array) - ::marker
// -- the rule li::before::marker, the array would still be [after, marker],
// so matching would fail at after (from array) - ::before.
ElementResolveContext::PseudoElementAncestors
ElementResolveContext::BuildPseudoElementAncestors(Element* element) {
  PseudoElementAncestors pseudo_element_ancestors;
  if (!element->IsPseudoElement()) {
    return pseudo_element_ancestors;
  }
  while (element->IsPseudoElement()) {
    CHECK_GE(pseudo_element_ancestors_size_, 0u);
    pseudo_element_ancestors[--pseudo_element_ancestors_size_] = element;
    element = element->parentElement();
  }
  DCHECK(element);
  DCHECK(!element->IsPseudoElement());

  return pseudo_element_ancestors;
}

namespace {
EInsideLink GetLinkStateForElement(Element& element) {
  if (!element.GetDocument().IsActive()) {
    // When requested from SelectorQuery, element can be in inactive document.
    return EInsideLink::kNotInsideLink;
  }

  bool force_visited = false;
  probe::ForcePseudoState(&element, CSSSelector::kPseudoVisited,
                          &force_visited);
  if (force_visited) {
    return EInsideLink::kInsideVisitedLink;
  }

  bool force_link = false;
  probe::ForcePseudoState(&element, CSSSelector::kPseudoLink, &force_link);
  if (force_link) {
    return EInsideLink::kInsideUnvisitedLink;
  }

  return element.GetDocument().GetVisitedLinkState().DetermineLinkState(
      element);
}
}  // namespace

ElementResolveContext::ElementResolveContext(Element& element)
    : element_(&element),
      ultimate_originating_element_(
          element_->IsPseudoElement()
              ? To<PseudoElement>(element_)->UltimateOriginatingElement()
              : element_),
      pseudo_element_(element_->IsPseudoElement() ? element_ : nullptr),
      element_link_state_(GetLinkStateForElement(element)),
      pseudo_element_ancestors_(BuildPseudoElementAncestors(&element)) {
  parent_element_ = LayoutTreeBuilderTraversal::ParentElement(element);
  layout_parent_ = LayoutTreeBuilderTraversal::LayoutParentElement(element);

  if (const Element* root_element = element.GetDocument().documentElement()) {
    if (element != root_element) {
      root_element_style_ = root_element->GetComputedStyle();
    }
  }
}

}  // namespace blink
