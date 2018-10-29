/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

using namespace HTMLNames;

static void DeflateIfOverlapped(LayoutRect&, LayoutRect&);
static bool IsScrollableNode(const Node*);

FocusCandidate::FocusCandidate(Node* node, WebFocusType direction)
    : visible_node(nullptr),
      focusable_node(nullptr),
      enclosing_scrollable_box(nullptr),
      distance(MaxDistance()),
      is_offscreen(true),
      is_offscreen_after_scrolling(true) {
  DCHECK(node);
  DCHECK(node->IsElementNode());

  if (auto* area = ToHTMLAreaElementOrNull(*node)) {
    HTMLImageElement* image = area->ImageElement();
    if (!image || !image->GetLayoutObject())
      return;

    visible_node = image;
    rect_in_root_frame = StartEdgeForAreaElement(*area, direction);
  } else {
    if (!node->GetLayoutObject())
      return;

    visible_node = node;
    rect_in_root_frame = NodeRectInRootFrame(node, true /* ignore border */);
  }

  focusable_node = node;
  is_offscreen = IsOffscreen(visible_node);
  is_offscreen_after_scrolling =
      IsOffscreenAfterFrameScroll(visible_node, direction);
}

bool IsSpatialNavigationEnabled(const LocalFrame* frame) {
  return (frame && frame->GetSettings() &&
          frame->GetSettings()->GetSpatialNavigationEnabled());
}

bool SpatialNavigationIgnoresEventHandlers(const LocalFrame* frame) {
  return (frame && frame->GetSettings() &&
          frame->GetSettings()->GetDeviceSupportsTouch());
}

static bool RectsIntersectOnOrthogonalAxis(WebFocusType direction,
                                           const LayoutRect& a,
                                           const LayoutRect& b) {
  switch (direction) {
    case kWebFocusTypeLeft:
    case kWebFocusTypeRight:
      return a.MaxY() > b.Y() && a.Y() < b.MaxY();
    case kWebFocusTypeUp:
    case kWebFocusTypeDown:
      return a.MaxX() > b.X() && a.X() < b.MaxX();
    default:
      NOTREACHED();
      return false;
  }
}

// Return true if rect |a| is below |b|. False otherwise.
// For overlapping rects, |a| is considered to be below |b|
// if both edges of |a| are below the respective ones of |b|.
static inline bool Below(const LayoutRect& a, const LayoutRect& b) {
  return a.Y() >= b.MaxY() || (a.Y() >= b.Y() && a.MaxY() > b.MaxY() &&
                               a.X() < b.MaxX() && a.MaxX() > b.X());
}

// Return true if rect |a| is on the right of |b|. False otherwise.
// For overlapping rects, |a| is considered to be on the right of |b|
// if both edges of |a| are on the right of the respective ones of |b|.
static inline bool RightOf(const LayoutRect& a, const LayoutRect& b) {
  return a.X() >= b.MaxX() || (a.X() >= b.X() && a.MaxX() > b.MaxX() &&
                               a.Y() < b.MaxY() && a.MaxY() > b.Y());
}

static bool IsRectInDirection(WebFocusType direction,
                              const LayoutRect& cur_rect,
                              const LayoutRect& target_rect) {
  switch (direction) {
    case kWebFocusTypeLeft:
      return RightOf(cur_rect, target_rect);
    case kWebFocusTypeRight:
      return RightOf(target_rect, cur_rect);
    case kWebFocusTypeUp:
      return Below(cur_rect, target_rect);
    case kWebFocusTypeDown:
      return Below(target_rect, cur_rect);
    default:
      NOTREACHED();
      return false;
  }
}

// Answers true if |node| is completely outside the user's (visual) viewport.
// This logic is used by spatnav to rule out offscreen focus candidates and an
// offscreen activeElement. When activeElement is offscreen, spatnav doesn't use
// it as the search origin; the search will start at an edge of the visual
// viewport instead.
// TODO(crbug.com/889840): Fix VisibleBoundsInVisualViewport().
// If VisibleBoundsInVisualViewport() would have taken "element-clips" into
// account, spatnav could have called it directly; no need to check the
// LayoutObject's VisibleContentRect.
bool IsOffscreen(const Node* node) {
  LocalFrameView* frame_view = node->GetDocument().View();
  if (!frame_view)
    return true;

  DCHECK(!frame_view->NeedsLayout());

  LayoutRect frame_viewport(
      frame_view->GetScrollableArea()->VisibleContentRect());

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return true;

  LayoutRect rect(layout_object->VisualRectInDocument());
  if (rect.IsEmpty())
    return true;

  if (!frame_viewport.Intersects(rect))
    return true;

  // Now we know that the node is visible in the its own frame's viewport (it is
  // not clipped by a scrollable div). That is, we've taken "element-clipping"
  // into account - now we only need to ensure that this node isn't clipped by
  // a frame.
  IntRect rect_in_root_frame;
  if (auto* document = DynamicTo<Document>(node))
    node = document->body();
  if (node->IsElementNode())
    rect_in_root_frame = ToElement(*node).VisibleBoundsInVisualViewport();
  return rect_in_root_frame.IsEmpty();
}

// As IsOffscreen() but returns visibility through the |node|'s frame's viewport
// after scrolling the frame in |direction|.
bool IsOffscreenAfterFrameScroll(const Node* node, WebFocusType direction) {
  LocalFrameView* frame_view = node->GetDocument().View();
  if (!frame_view)
    return true;

  DCHECK(!frame_view->NeedsLayout());

  // If |node| is in the root frame, VisibleContentRect() will include
  // visual viewport transformation (pinch-zoom) if one exists.
  LayoutRect frame_viewport(
      frame_view->GetScrollableArea()->VisibleContentRect());

  // |direction| extends the node's frame's viewport's rect (before doing the
  // intersection-check) to also include content revealed by one scroll step in
  // that |direction|.
  int pixels_per_line_step =
      ScrollableArea::PixelsPerLineStep(frame_view->GetChromeClient());
  switch (direction) {
    case kWebFocusTypeLeft:
      frame_viewport.SetX(frame_viewport.X() - pixels_per_line_step);
      frame_viewport.SetWidth(frame_viewport.Width() + pixels_per_line_step);
      break;
    case kWebFocusTypeRight:
      frame_viewport.SetWidth(frame_viewport.Width() + pixels_per_line_step);
      break;
    case kWebFocusTypeUp:
      frame_viewport.SetY(frame_viewport.Y() - pixels_per_line_step);
      frame_viewport.SetHeight(frame_viewport.Height() + pixels_per_line_step);
      break;
    case kWebFocusTypeDown:
      frame_viewport.SetHeight(frame_viewport.Height() + pixels_per_line_step);
      break;
    default:
      break;
  }

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return true;

  LayoutRect rect(layout_object->VisualRectInDocument());
  if (rect.IsEmpty())
    return true;

  return !frame_viewport.Intersects(rect);
}

bool HasRemoteFrame(const Node* node) {
  if (!node)
    return false;

  return node->IsFrameOwnerElement() &&
         ToHTMLFrameOwnerElement(node)->ContentFrame() &&
         ToHTMLFrameOwnerElement(node)->ContentFrame()->IsRemoteFrame();
}

bool ScrollInDirection(LocalFrame* frame, WebFocusType direction) {
  DCHECK(frame);

  if (frame && CanScrollInDirection(frame->GetDocument(), direction)) {
    int dx = 0;
    int dy = 0;
    int pixels_per_line_step =
        ScrollableArea::PixelsPerLineStep(frame->View()->GetChromeClient());
    switch (direction) {
      case kWebFocusTypeLeft:
        dx = -pixels_per_line_step;
        break;
      case kWebFocusTypeRight:
        dx = pixels_per_line_step;
        break;
      case kWebFocusTypeUp:
        dy = -pixels_per_line_step;
        break;
      case kWebFocusTypeDown:
        dy = pixels_per_line_step;
        break;
      default:
        NOTREACHED();
        return false;
    }

    frame->View()->GetScrollableArea()->ScrollBy(ScrollOffset(dx, dy),
                                                 kUserScroll);
    return true;
  }
  return false;
}

bool ScrollInDirection(Node* container, WebFocusType direction) {
  DCHECK(container);
  if (auto* document = DynamicTo<Document>(container))
    return ScrollInDirection(document->GetFrame(), direction);

  if (!container->GetLayoutBox())
    return false;

  if (CanScrollInDirection(container, direction)) {
    int dx = 0;
    int dy = 0;
    // TODO(leviw): Why are these values truncated (toInt) instead of rounding?
    LocalFrameView* frame_view = container->GetDocument().View();
    int pixels_per_line_step = ScrollableArea::PixelsPerLineStep(
        frame_view ? frame_view->GetChromeClient() : nullptr);
    switch (direction) {
      case kWebFocusTypeLeft:
        dx = -pixels_per_line_step;
        break;
      case kWebFocusTypeRight:
        DCHECK_GT(container->GetLayoutBox()->ScrollWidth(),
                  (container->GetLayoutBox()->ScrollLeft() +
                   container->GetLayoutBox()->ClientWidth()));
        dx = pixels_per_line_step;
        break;
      case kWebFocusTypeUp:
        dy = -pixels_per_line_step;
        break;
      case kWebFocusTypeDown:
        DCHECK(container->GetLayoutBox()->ScrollHeight() -
               (container->GetLayoutBox()->ScrollTop() +
                container->GetLayoutBox()->ClientHeight()));
        dy = pixels_per_line_step;
        break;
      default:
        NOTREACHED();
        return false;
    }

    container->GetLayoutBox()->ScrollByRecursively(ScrollOffset(dx, dy));
    return true;
  }

  return false;
}

static void DeflateIfOverlapped(LayoutRect& a, LayoutRect& b) {
  if (!a.Intersects(b) || a.Contains(b) || b.Contains(a))
    return;

  LayoutUnit deflate_factor = LayoutUnit(-FudgeFactor());

  // Avoid negative width or height values.
  if ((a.Width() + 2 * deflate_factor > 0) &&
      (a.Height() + 2 * deflate_factor > 0))
    a.Inflate(deflate_factor);

  if ((b.Width() + 2 * deflate_factor > 0) &&
      (b.Height() + 2 * deflate_factor > 0))
    b.Inflate(deflate_factor);
}

bool IsScrollableNode(const Node* node) {
  DCHECK(!node->IsDocumentNode());

  if (!node)
    return false;

  if (LayoutObject* layout_object = node->GetLayoutObject())
    return layout_object->IsBox() &&
           ToLayoutBox(layout_object)->CanBeScrolledAndHasScrollableArea() &&
           node->hasChildren();

  return false;
}

Node* ScrollableAreaOrDocumentOf(Node* node) {
  DCHECK(node);
  Node* parent = node;
  do {
    // FIXME: Spatial navigation is broken for OOPI.
    if (auto* document = DynamicTo<Document>(parent))
      parent = document->GetFrame()->DeprecatedLocalOwner();
    else
      parent = parent->ParentOrShadowHostNode();
  } while (parent && !IsScrollableAreaOrDocument(parent));

  return parent;
}

bool IsScrollableAreaOrDocument(const Node* node) {
  if (!node)
    return false;

  return node->IsDocumentNode() ||
         (node->IsFrameOwnerElement() &&
          ToHTMLFrameOwnerElement(node)->ContentFrame()) ||
         IsScrollableNode(node);
}

bool IsNavigableContainer(const Node* node, WebFocusType direction) {
  if (!node)
    return false;

  return node->IsDocumentNode() ||
         (node->IsFrameOwnerElement() &&
          ToHTMLFrameOwnerElement(node)->ContentFrame()) ||
         CanScrollInDirection(node, direction);
}

bool CanScrollInDirection(const Node* container, WebFocusType direction) {
  DCHECK(container);
  if (auto* document = DynamicTo<Document>(container))
    return CanScrollInDirection(document->GetFrame(), direction);

  if (!IsScrollableNode(container))
    return false;

  switch (direction) {
    case kWebFocusTypeLeft:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              container->GetLayoutBox()->ScrollLeft() > 0);
    case kWebFocusTypeUp:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              container->GetLayoutBox()->ScrollTop() > 0);
    case kWebFocusTypeRight:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              container->GetLayoutBox()->ScrollLeft() +
                      container->GetLayoutBox()->ClientWidth() <
                  container->GetLayoutBox()->ScrollWidth());
    case kWebFocusTypeDown:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              container->GetLayoutBox()->ScrollTop() +
                      container->GetLayoutBox()->ClientHeight() <
                  container->GetLayoutBox()->ScrollHeight());
    default:
      NOTREACHED();
      return false;
  }
}

bool CanScrollInDirection(const LocalFrame* frame, WebFocusType direction) {
  if (!frame->View())
    return false;
  LayoutView* layoutView = frame->ContentLayoutObject();
  if (!layoutView)
    return false;
  ScrollbarMode vertical_mode;
  ScrollbarMode horizontal_mode;
  layoutView->CalculateScrollbarModes(horizontal_mode, vertical_mode);
  if ((direction == kWebFocusTypeLeft || direction == kWebFocusTypeRight) &&
      kScrollbarAlwaysOff == horizontal_mode)
    return false;
  if ((direction == kWebFocusTypeUp || direction == kWebFocusTypeDown) &&
      kScrollbarAlwaysOff == vertical_mode)
    return false;
  ScrollableArea* scrollable_area = frame->View()->GetScrollableArea();
  LayoutSize size(scrollable_area->ContentsSize());
  LayoutSize offset(scrollable_area->ScrollOffsetInt());
  LayoutRect rect(scrollable_area->VisibleContentRect(kIncludeScrollbars));

  switch (direction) {
    case kWebFocusTypeLeft:
      return offset.Width() > 0;
    case kWebFocusTypeUp:
      return offset.Height() > 0;
    case kWebFocusTypeRight:
      return rect.Width() + offset.Width() < size.Width();
    case kWebFocusTypeDown:
      return rect.Height() + offset.Height() < size.Height();
    default:
      NOTREACHED();
      return false;
  }
}

LayoutRect NodeRectInRootFrame(const Node* node, bool ignore_border) {
  DCHECK(node);
  DCHECK(node->GetLayoutObject());
  DCHECK(!node->GetDocument().View()->NeedsLayout());

  LayoutRect rect = node->GetDocument().GetFrame()->View()->ConvertToRootFrame(
      node->BoundingBox());

  // For authors that use border instead of outline in their CSS, we compensate
  // by ignoring the border when calculating the rect of the focused element.
  if (ignore_border) {
    rect.Move(node->GetLayoutObject()->Style()->BorderLeftWidth(),
              node->GetLayoutObject()->Style()->BorderTopWidth());
    rect.SetWidth(LayoutUnit(
        rect.Width() - node->GetLayoutObject()->Style()->BorderLeftWidth() -
        node->GetLayoutObject()->Style()->BorderRightWidth()));
    rect.SetHeight(LayoutUnit(
        rect.Height() - node->GetLayoutObject()->Style()->BorderTopWidth() -
        node->GetLayoutObject()->Style()->BorderBottomWidth()));
  }
  return rect;
}

// This method calculates the exitPoint from the startingRect and the entryPoint
// into the candidate rect.  The line between those 2 points is the closest
// distance between the 2 rects.  Takes care of overlapping rects, defining
// points so that the distance between them is zero where necessary.
void EntryAndExitPointsForDirection(WebFocusType direction,
                                    const LayoutRect& starting_rect,
                                    const LayoutRect& potential_rect,
                                    LayoutPoint& exit_point,
                                    LayoutPoint& entry_point) {
  switch (direction) {
    case kWebFocusTypeLeft:
      exit_point.SetX(starting_rect.X());
      if (potential_rect.MaxX() < starting_rect.X())
        entry_point.SetX(potential_rect.MaxX());
      else
        entry_point.SetX(starting_rect.X());
      break;
    case kWebFocusTypeUp:
      exit_point.SetY(starting_rect.Y());
      if (potential_rect.MaxY() < starting_rect.Y())
        entry_point.SetY(potential_rect.MaxY());
      else
        entry_point.SetY(starting_rect.Y());
      break;
    case kWebFocusTypeRight:
      exit_point.SetX(starting_rect.MaxX());
      if (potential_rect.X() > starting_rect.MaxX())
        entry_point.SetX(potential_rect.X());
      else
        entry_point.SetX(starting_rect.MaxX());
      break;
    case kWebFocusTypeDown:
      exit_point.SetY(starting_rect.MaxY());
      if (potential_rect.Y() > starting_rect.MaxY())
        entry_point.SetY(potential_rect.Y());
      else
        entry_point.SetY(starting_rect.MaxY());
      break;
    default:
      NOTREACHED();
  }

  switch (direction) {
    case kWebFocusTypeLeft:
    case kWebFocusTypeRight:
      if (Below(starting_rect, potential_rect)) {
        exit_point.SetY(starting_rect.Y());
        if (potential_rect.MaxY() < starting_rect.Y())
          entry_point.SetY(potential_rect.MaxY());
        else
          entry_point.SetY(starting_rect.Y());
      } else if (Below(potential_rect, starting_rect)) {
        exit_point.SetY(starting_rect.MaxY());
        if (potential_rect.Y() > starting_rect.MaxY())
          entry_point.SetY(potential_rect.Y());
        else
          entry_point.SetY(starting_rect.MaxY());
      } else {
        exit_point.SetY(max(starting_rect.Y(), potential_rect.Y()));
        entry_point.SetY(exit_point.Y());
      }
      break;
    case kWebFocusTypeUp:
    case kWebFocusTypeDown:
      if (RightOf(starting_rect, potential_rect)) {
        exit_point.SetX(starting_rect.X());
        if (potential_rect.MaxX() < starting_rect.X())
          entry_point.SetX(potential_rect.MaxX());
        else
          entry_point.SetX(starting_rect.X());
      } else if (RightOf(potential_rect, starting_rect)) {
        exit_point.SetX(starting_rect.MaxX());
        if (potential_rect.X() > starting_rect.MaxX())
          entry_point.SetX(potential_rect.X());
        else
          entry_point.SetX(starting_rect.MaxX());
      } else {
        exit_point.SetX(max(starting_rect.X(), potential_rect.X()));
        entry_point.SetX(exit_point.X());
      }
      break;
    default:
      NOTREACHED();
  }
}

bool AreElementsOnSameLine(const FocusCandidate& first_candidate,
                           const FocusCandidate& second_candidate) {
  if (first_candidate.IsNull() || second_candidate.IsNull())
    return false;

  if (!first_candidate.visible_node->GetLayoutObject() ||
      !second_candidate.visible_node->GetLayoutObject())
    return false;

  if (!first_candidate.rect_in_root_frame.Intersects(
          second_candidate.rect_in_root_frame))
    return false;

  if (IsHTMLAreaElement(*first_candidate.focusable_node) ||
      IsHTMLAreaElement(*second_candidate.focusable_node))
    return false;

  if (!first_candidate.visible_node->GetLayoutObject()->IsLayoutInline() ||
      !second_candidate.visible_node->GetLayoutObject()->IsLayoutInline())
    return false;

  if (first_candidate.visible_node->GetLayoutObject()->ContainingBlock() !=
      second_candidate.visible_node->GetLayoutObject()->ContainingBlock())
    return false;

  return true;
}

void DistanceDataForNode(WebFocusType direction,
                         const FocusCandidate& current,
                         FocusCandidate& candidate) {
  if (!IsRectInDirection(direction, current.rect_in_root_frame,
                         candidate.rect_in_root_frame))
    return;

  if (AreElementsOnSameLine(current, candidate)) {
    if ((direction == kWebFocusTypeUp &&
         current.rect_in_root_frame.Y() > candidate.rect_in_root_frame.Y()) ||
        (direction == kWebFocusTypeDown &&
         candidate.rect_in_root_frame.Y() > current.rect_in_root_frame.Y())) {
      candidate.distance = 0;
      return;
    }
  }

  LayoutRect node_rect = candidate.rect_in_root_frame;
  LayoutRect current_rect = current.rect_in_root_frame;
  DeflateIfOverlapped(current_rect, node_rect);

  LayoutPoint exit_point;
  LayoutPoint entry_point;
  EntryAndExitPointsForDirection(direction, current_rect, node_rect, exit_point,
                                 entry_point);

  LayoutUnit x_axis = (exit_point.X() - entry_point.X()).Abs();
  LayoutUnit y_axis = (exit_point.Y() - entry_point.Y()).Abs();

  LayoutUnit navigation_axis_distance;
  LayoutUnit weighted_orthogonal_axis_distance;

  // Bias and weights are put to the orthogonal axis distance calculation
  // so aligned candidates would have advantage over partially-aligned ones
  // and then over not-aligned candidates. The bias is given to not-aligned
  // candidates with respect to size of the current rect. The weight for
  // left/right direction is given a higher value to allow navigation on
  // common horizonally-aligned elements. The hardcoded values are based on
  // tests and experiments.
  const int kOrthogonalWeightForLeftRight = 30;
  const int kOrthogonalWeightForUpDown = 2;
  int orthogonal_bias = 0;

  switch (direction) {
    case kWebFocusTypeLeft:
    case kWebFocusTypeRight:
      navigation_axis_distance = x_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Height() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (y_axis + orthogonal_bias) * kOrthogonalWeightForLeftRight;
      break;
    case kWebFocusTypeUp:
    case kWebFocusTypeDown:
      navigation_axis_distance = y_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Width() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (x_axis + orthogonal_bias) * kOrthogonalWeightForUpDown;
      break;
    default:
      NOTREACHED();
      return;
  }

  double euclidian_distance_pow2 =
      (x_axis * x_axis + y_axis * y_axis).ToDouble();
  LayoutRect intersection_rect = Intersection(current_rect, node_rect);
  double overlap =
      (intersection_rect.Width() * intersection_rect.Height()).ToDouble();

  // Distance calculation is based on http://www.w3.org/TR/WICD/#focus-handling
  candidate.distance = sqrt(euclidian_distance_pow2) +
                       navigation_axis_distance +
                       weighted_orthogonal_axis_distance - sqrt(overlap);
}

bool CanBeScrolledIntoView(WebFocusType direction,
                           const FocusCandidate& candidate) {
  DCHECK(candidate.visible_node);
  DCHECK(candidate.is_offscreen);
  LayoutRect candidate_rect = candidate.rect_in_root_frame;
  // TODO(ecobos@igalia.com): Investigate interaction with Shadow DOM.
  for (Node& parent_node :
       NodeTraversal::AncestorsOf(*candidate.visible_node)) {
    if (UNLIKELY(!parent_node.GetLayoutObject())) {
      DCHECK(parent_node.IsElementNode() &&
             ToElement(parent_node).HasDisplayContentsStyle());
      continue;
    }

    LayoutRect parent_rect = NodeRectInRootFrame(&parent_node);
    if (!candidate_rect.Intersects(parent_rect)) {
      if (((direction == kWebFocusTypeLeft ||
            direction == kWebFocusTypeRight) &&
           parent_node.GetLayoutObject()->Style()->OverflowX() ==
               EOverflow::kHidden) ||
          ((direction == kWebFocusTypeUp || direction == kWebFocusTypeDown) &&
           parent_node.GetLayoutObject()->Style()->OverflowY() ==
               EOverflow::kHidden))
        return false;
    }
    if (parent_node == candidate.enclosing_scrollable_box)
      return CanScrollInDirection(&parent_node, direction);
  }
  return true;
}

// Returns a thin rectangle that represents one of box's sides.
LayoutRect OppositeEdge(WebFocusType side,
                        const LayoutRect& box,
                        LayoutUnit thickness) {
  LayoutRect thin_rect = box;
  switch (side) {
    case kWebFocusTypeLeft:
      thin_rect.SetX(thin_rect.MaxX() - thickness);
      thin_rect.SetWidth(thickness);
      break;
    case kWebFocusTypeRight:
      thin_rect.SetWidth(thickness);
      break;
    case kWebFocusTypeDown:
      thin_rect.SetHeight(thickness);
      break;
    case kWebFocusTypeUp:
      thin_rect.SetY(thin_rect.MaxY() - thickness);
      thin_rect.SetHeight(thickness);
      break;
    default:
      NOTREACHED();
  }

  return thin_rect;
}

LayoutRect StartEdgeForAreaElement(const HTMLAreaElement& area,
                                   WebFocusType direction) {
  DCHECK(area.ImageElement());
  // Area elements tend to overlap more than other focusable elements. We
  // flatten the rect of the area elements to minimize the effect of overlapping
  // areas.
  LayoutRect rect = OppositeEdge(
      direction,
      area.GetDocument().GetFrame()->View()->ConvertToRootFrame(
          area.ComputeAbsoluteRect(area.ImageElement()->GetLayoutObject())),
      LayoutUnit(1) /* snav-imagemap-overlapped-areas.html */);
  return rect;
}

HTMLFrameOwnerElement* FrameOwnerElement(FocusCandidate& candidate) {
  return candidate.IsFrameOwnerElement()
             ? ToHTMLFrameOwnerElement(candidate.visible_node)
             : nullptr;
};

// The visual viewport's rect (given in the root frame's coordinate space).
LayoutRect RootViewport(const LocalFrame* current_frame) {
  return LayoutRect(
      current_frame->GetPage()->GetVisualViewport().VisibleRect());
}

// Spatnav uses this rectangle to measure distances to focus candidates.
// The search origin is either activeElement F itself, if it's being at least
// partially visible, or else, its first [partially] visible scroller. If both
// F and its enclosing scroller are completely off-screen, we recurse to the
// scroller’s scroller ... all the way up until the root frame's document.
// The root frame's document is a good base case because it's, per definition,
// a visible scrollable area.
LayoutRect SearchOrigin(const LayoutRect viewport_rect_of_root_frame,
                        Node* focus_node,
                        const WebFocusType direction) {
  if (!focus_node) {
    // Search from one of the visual viewport's edges towards the navigated
    // direction. For example, UP makes spatnav search upwards, starting at the
    // visual viewport's bottom.
    return OppositeEdge(direction, viewport_rect_of_root_frame);
  }

  auto* area_element = ToHTMLAreaElementOrNull(focus_node);
  if (area_element)
    focus_node = area_element->ImageElement();

  if (!IsOffscreen(focus_node)) {
    if (area_element)
      return StartEdgeForAreaElement(*area_element, direction);

    LayoutRect box_in_root_frame = NodeRectInRootFrame(focus_node, true);
    return Intersection(box_in_root_frame, viewport_rect_of_root_frame);
  }

  Node* container = ScrollableAreaOrDocumentOf(focus_node);
  while (container) {
    if (!IsOffscreen(container)) {
      // The first scroller that encloses focus and is [partially] visible.
      LayoutRect box_in_root_frame = NodeRectInRootFrame(container, true);
      return OppositeEdge(direction, Intersection(box_in_root_frame,
                                                  viewport_rect_of_root_frame));
    }

    container = ScrollableAreaOrDocumentOf(container);
  }

  return OppositeEdge(direction, viewport_rect_of_root_frame);
}

}  // namespace blink
