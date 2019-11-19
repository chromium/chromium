/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/visited_link_state.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"

namespace blink {

static inline const AtomicString& LinkAttribute(const Element& element) {
  DCHECK(element.IsLink());
  if (element.IsHTMLElement())
    return element.FastGetAttribute(html_names::kHrefAttr);
  DCHECK(element.IsSVGElement());
  return SVGURIReference::LegacyHrefString(To<SVGElement>(element));
}

static inline LinkHash LinkHashForElement(
    const Element& element,
    const AtomicString& attribute = AtomicString()) {
  DCHECK(attribute.IsNull() || LinkAttribute(element) == attribute);
  if (auto* anchor = DynamicTo<HTMLAnchorElement>(element))
    return anchor->VisitedLinkHash();
  return VisitedLinkHash(
      element.GetDocument().BaseURL(),
      attribute.IsNull() ? LinkAttribute(element) : attribute);
}

VisitedLinkState::VisitedLinkState(const Document& document)
    : document_(document) {}

static void InvalidateStyleForAllLinksRecursively(
    Node& root_node,
    bool invalidate_visited_link_hashes) {
  for (Node& node : NodeTraversal::StartsAt(root_node)) {
    if (node.IsLink()) {
      auto* html_anchor_element = DynamicTo<HTMLAnchorElement>(node);
      if (invalidate_visited_link_hashes && html_anchor_element)
        html_anchor_element->InvalidateCachedVisitedLinkHash();
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoLink);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoVisited);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    if (ShadowRoot* root = node.GetShadowRoot()) {
      InvalidateStyleForAllLinksRecursively(*root,
                                            invalidate_visited_link_hashes);
    }
  }
}

void VisitedLinkState::InvalidateStyleForAllLinks(
    bool invalidate_visited_link_hashes) {
  if (!links_checked_for_visited_state_.IsEmpty() && GetDocument().firstChild())
    InvalidateStyleForAllLinksRecursively(*GetDocument().firstChild(),
                                          invalidate_visited_link_hashes);
}

static void InvalidateStyleForLinkRecursively(Node& root_node,
                                              LinkHash link_hash) {
  for (Node& node : NodeTraversal::StartsAt(root_node)) {
    if (node.IsLink() && LinkHashForElement(To<Element>(node)) == link_hash) {
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoLink);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoVisited);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      To<Element>(node).PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    if (ShadowRoot* root = node.GetShadowRoot())
      InvalidateStyleForLinkRecursively(*root, link_hash);
  }
}

void VisitedLinkState::InvalidateStyleForLink(LinkHash link_hash) {
  if (links_checked_for_visited_state_.Contains(link_hash) &&
      GetDocument().firstChild())
    InvalidateStyleForLinkRecursively(*GetDocument().firstChild(), link_hash);
}

EInsideLink VisitedLinkState::DetermineLinkStateSlowCase(
    const Element& element) {
  DCHECK(element.IsLink());
  DCHECK(GetDocument().IsActive());
  DCHECK(GetDocument() == element.GetDocument());

  const AtomicString& attribute = LinkAttribute(element);

  if (attribute.IsNull())
    return EInsideLink::kNotInsideLink;  // This can happen for <img usemap>

  // An empty attribute refers to the document itself which is always
  // visited. It is useful to check this explicitly so that visited
  // links can be tested in platform independent manner, without
  // explicit support in the test harness.
  if (attribute.IsEmpty())
    return EInsideLink::kInsideVisitedLink;

  if (LinkHash hash = LinkHashForElement(element, attribute)) {
    links_checked_for_visited_state_.insert(hash);
    if (Platform::Current()->IsLinkVisited(hash))
      return EInsideLink::kInsideVisitedLink;
  }

  return EInsideLink::kInsideUnvisitedLink;
}

void VisitedLinkState::Trace(Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
