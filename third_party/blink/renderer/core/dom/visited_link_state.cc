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

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"

namespace blink {

static inline const SecurityOrigin* CalculateFrameOrigin(
    const Document& document) {
  // Obtain the SecurityOrigin for our Document as a url::Origin.
  // NOTE: for all Documents which have a valid VisitedLinkState, we should not
  // ever encounter an invalid `window` or `security_origin`.
  const LocalDOMWindow* window = document.domWindow();
  DCHECK(window);
  return window->GetSecurityOrigin();
}

static inline const AtomicString& LinkAttribute(const Element& element) {
  DCHECK(element.IsLink());
  if (element.IsHTMLElement())
    return element.FastGetAttribute(html_names::kHrefAttr);
  DCHECK(element.IsSVGElement());
  return SVGURIReference::LegacyHrefString(To<SVGElement>(element));
}

static inline LinkHash UnpartitionedLinkHashForElement(
    const Element& element,
    const AtomicString& attribute) {
  // TODO(crbug.com/369219144): Should this be DynamicTo<HTMLAnchorElementBase>?
  if (auto* anchor = DynamicTo<HTMLAnchorElement>(element))
    return anchor->VisitedLinkHash();
  return VisitedLinkHash(
      element.GetDocument().BaseURL(),
      attribute.IsNull() ? LinkAttribute(element) : attribute);
}

static inline LinkHash PartitionedLinkHashForElement(
    const Element& element,
    const AtomicString& attribute) {
  // TODO(crbug.com/369219144): Should this be DynamicTo<HTMLAnchorElementBase>?
  if (auto* anchor = DynamicTo<HTMLAnchorElement>(element)) {
    return anchor->PartitionedVisitedLinkFingerprint();
  }
  // Obtain the parameters of our triple-partition key.
  // (1) Link URL (base and relative).
  const KURL base_link_url = element.GetDocument().BaseURL();
  const AtomicString relative_link_url =
      attribute.IsNull() ? LinkAttribute(element) : attribute;
  // (2) Top-Level Site.
  // NOTE: for all Documents which have a valid VisitedLinkState, we should not
  // ever encounter an invalid GetFrame() or an invalid TopFrameOrigin().
  DCHECK(element.GetDocument().TopFrameOrigin());
  const net::SchemefulSite top_level_site(
      element.GetDocument().TopFrameOrigin()->ToUrlOrigin());
  // (3) Frame Origin.
  const SecurityOrigin* frame_origin =
      CalculateFrameOrigin(element.GetDocument());

  // Calculate the fingerprint for this :visited link and return its value.
  // NOTE: In third_party/blink/renderer/ code, this fingerprint value will
  // sometimes be referred to as a LinkHash.
  return PartitionedVisitedLinkFingerprint(base_link_url, relative_link_url,
                                           top_level_site, frame_origin);
}

static inline LinkHash LinkHashForElement(
    const Element& element,
    const AtomicString& attribute = AtomicString()) {
  DCHECK(attribute.IsNull() || LinkAttribute(element) == attribute);
  return base::FeatureList::IsEnabled(
             blink::features::kPartitionVisitedLinkDatabase) ||
                 base::FeatureList::IsEnabled(
                     blink::features::
                         kPartitionVisitedLinkDatabaseWithSelfLinks)
             ? PartitionedLinkHashForElement(element, attribute)
             : UnpartitionedLinkHashForElement(element, attribute);
}

VisitedLinkState::VisitedLinkState(const Document& document)
    : document_(document) {}

static void InvalidateStyleForAllLinksRecursively(
    Node& root_node,
    bool invalidate_visited_link_hashes) {
  for (Node& node : NodeTraversal::StartsAt(root_node)) {
    if (node.IsLink()) {
      // TODO(crbug.com/369219144): Should this be
      // DynamicTo<HTMLAnchorElementBase>?
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
  if (!links_checked_for_visited_state_.empty() && GetDocument().firstChild())
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
    if (ShadowRoot* root = node.GetShadowRoot()) {
      InvalidateStyleForLinkRecursively(*root, link_hash);
    }
  }
}

void VisitedLinkState::InvalidateStyleForLink(LinkHash link_hash) {
  if (links_checked_for_visited_state_.Contains(link_hash) &&
      GetDocument().firstChild())
    InvalidateStyleForLinkRecursively(*GetDocument().firstChild(), link_hash);
}

void VisitedLinkState::UpdateSalt(uint64_t visited_link_salt) {
  // Inform VisitedLinkReader in our corresponding process of new salt value.
  Platform::Current()->AddOrUpdateVisitedLinkSalt(
      CalculateFrameOrigin(GetDocument())->ToUrlOrigin(), visited_link_salt);
}

EInsideLink VisitedLinkState::DetermineLinkStateSlowCase(
    const Element& element) {
  DCHECK(element.IsLink());
  DCHECK(GetDocument().IsActive());
  DCHECK(GetDocument() == element.GetDocument());

  const AtomicString& attribute = LinkAttribute(element);

  if (attribute.IsNull())
    return EInsideLink::kNotInsideLink;  // This can happen for <img usemap>

  // Cache the feature status to avoid frequent calculation.
  static const bool are_partitioned_visited_links_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) ||
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks);

  if (are_partitioned_visited_links_enabled) {
    // In a partitioned :visited model, we don't want to display :visited-ness
    // inside credentialless iframes.
    if (GetDocument()
            .GetExecutionContext()
            ->GetPolicyContainer()
            ->GetPolicies()
            .is_credentialless) {
      return EInsideLink::kNotInsideLink;
    }
    // In a partitioned :visited model, we don't want to display :visited-ness
    // inside Fenced Frames or any frame which has a Fenced Frame in its
    // FrameTree.
    if (GetDocument().GetFrame()->IsInFencedFrameTree()) {
      UMA_HISTOGRAM_BOOLEAN("Blink.History.VisitedLinks.InFencedFrameTree",
                            true);
      return EInsideLink::kNotInsideLink;
    }
    // Record in our histogram that we are not in or a child of a Fenced Frame.
    UMA_HISTOGRAM_BOOLEAN("Blink.History.VisitedLinks.InFencedFrameTree",
                          false);
  }

  // An empty attribute refers to the document itself which is always
  // visited. It is useful to check this explicitly so that visited
  // links can be tested in platform independent manner, without
  // explicit support in the test harness.
  if (attribute.empty()) {
    base::UmaHistogramBoolean(
        "Blink.History.VisitedLinks.IsLinkStyledAsVisited", true);
    return EInsideLink::kInsideVisitedLink;
  }

  if (LinkHash hash = LinkHashForElement(element, attribute)) {
    links_checked_for_visited_state_.insert(hash);
    if (Platform::Current()->IsLinkVisited(hash)) {
      base::UmaHistogramBoolean(
          "Blink.History.VisitedLinks.IsLinkStyledAsVisited", true);
      return EInsideLink::kInsideVisitedLink;
    }
  }

  base::UmaHistogramBoolean("Blink.History.VisitedLinks.IsLinkStyledAsVisited",
                            false);
  return EInsideLink::kInsideUnvisitedLink;
}

void VisitedLinkState::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
