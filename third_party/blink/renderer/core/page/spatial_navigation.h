/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_H_

#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

#include <limits>

namespace blink {

class LocalFrame;
class HTMLAreaElement;
class HTMLFrameOwnerElement;

inline double MaxDistance() {
  return std::numeric_limits<double>::max();
}

inline int FudgeFactor() {
  return 2;
}

CORE_EXPORT bool IsSpatialNavigationEnabled(const LocalFrame*);
bool SpatialNavigationIgnoresEventHandlers(const LocalFrame*);

struct FocusCandidate {
  STACK_ALLOCATED();

 public:
  FocusCandidate()
      : visible_node(nullptr),
        focusable_node(nullptr),
        enclosing_scrollable_box(nullptr),
        distance(MaxDistance()),
        is_offscreen(true),
        is_offscreen_after_scrolling(true) {}

  FocusCandidate(Node*, WebFocusType);
  explicit FocusCandidate(HTMLAreaElement*, WebFocusType);
  bool IsNull() const { return !visible_node; }
  bool InScrollableContainer() const {
    return visible_node && enclosing_scrollable_box;
  }
  bool IsFrameOwnerElement() const {
    return visible_node && visible_node->IsFrameOwnerElement();
  }
  Document* GetDocument() const {
    return visible_node ? &visible_node->GetDocument() : nullptr;
  }

  // We handle differently visibleNode and FocusableNode to properly handle the
  // areas of imagemaps, where visibleNode would represent the image element and
  // focusableNode would represent the area element.  In all other cases,
  // visibleNode and focusableNode are one and the same.
  Member<Node> visible_node;
  Member<Node> focusable_node;
  Member<Node> enclosing_scrollable_box;
  double distance;
  LayoutRect rect_in_root_frame;
  bool is_offscreen;
  bool is_offscreen_after_scrolling;
};

CORE_EXPORT bool HasRemoteFrame(const Node*);
CORE_EXPORT bool IsOffscreen(const Node*);
CORE_EXPORT bool IsOffscreenAfterFrameScroll(const Node*, WebFocusType);
bool ScrollInDirection(LocalFrame*, WebFocusType);
bool ScrollInDirection(Node* container, WebFocusType);
bool IsNavigableContainer(const Node*, WebFocusType);
CORE_EXPORT bool IsScrollableAreaOrDocument(const Node*);
CORE_EXPORT Node* ScrollableAreaOrDocumentOf(Node*);
bool CanScrollInDirection(const Node* container, WebFocusType);
bool CanScrollInDirection(const LocalFrame*, WebFocusType);
bool CanBeScrolledIntoView(WebFocusType, const FocusCandidate&);
bool AreElementsOnSameLine(const FocusCandidate& first_candidate,
                           const FocusCandidate& second_candidate);
void DistanceDataForNode(WebFocusType,
                         const FocusCandidate& current,
                         FocusCandidate&);
CORE_EXPORT LayoutRect NodeRectInRootFrame(const Node*,
                                           bool ignore_border = false);
CORE_EXPORT LayoutRect OppositeEdge(WebFocusType side,
                                    const LayoutRect& box,
                                    LayoutUnit thickness = LayoutUnit());
CORE_EXPORT LayoutRect RootViewport(const LocalFrame*);
LayoutRect StartEdgeForAreaElement(const HTMLAreaElement&, WebFocusType);
HTMLFrameOwnerElement* FrameOwnerElement(FocusCandidate&);
CORE_EXPORT LayoutRect SearchOrigin(const LayoutRect,
                                    Node*,
                                    const WebFocusType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_H_
