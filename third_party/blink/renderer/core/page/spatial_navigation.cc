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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "base/containers/adapters.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// A small integer that easily fits into a double with a good margin for
// arithmetic. In particular, we don't want to use
// std::numeric_limits<double>::lowest() because, if subtracted, it becomes
// NaN which will make all following arithmetic NaN too (an unusable number).
constexpr double kMinDistance = std::numeric_limits<int>::lowest();
// Assign negative values to the distance value to give the candidate a higher
// priority.
// kPriorityClassA is for elements in separate layers such as pop-ups.
// kPriorityClassB is for intersecting elements.
constexpr double kPriorityClassA = kMinDistance / 2;
constexpr double kPriorityClassB = kMinDistance / 4;

constexpr int kFudgeFactor = 2;

FocusCandidate::FocusCandidate(Node* node, SpatialNavigationDirection direction)
    : visible_node(nullptr), focusable_node(nullptr), is_offscreen(true) {
  DCHECK(node);
  DCHECK(node->IsElementNode());

  if (auto* area = DynamicTo<HTMLAreaElement>(*node)) {
    HTMLImageElement* image = area->ImageElement();
    if (!image || !image->GetLayoutObject())
      return;

    visible_node = image;
    rect_in_root_frame = StartEdgeForAreaElement(*area, direction);
  } else {
    if (!node->GetLayoutObject())
      return;

    visible_node = node;
    rect_in_root_frame = NodeRectInRootFrame(node);

    // Remove any overlap with line boxes *above* the search origin.
    rect_in_root_frame =
        ShrinkInlineBoxToLineBox(*node->GetLayoutObject(), rect_in_root_frame);
  }

  focusable_node = node;
  is_offscreen = IsOffscreen(visible_node);
}

bool IsSpatialNavigationEnabled(const LocalFrame* frame) {
  return (frame && frame->GetSettings() &&
          frame->GetSettings()->GetSpatialNavigationEnabled());
}

static bool RectsIntersectOnOrthogonalAxis(SpatialNavigationDirection direction,
                                           const PhysicalRect& a,
                                           const PhysicalRect& b) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      return a.Bottom() > b.Y() && a.Y() < b.Bottom();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      return a.Right() > b.X() && a.X() < b.Right();
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// Determines if a candidate element is in a specific direction.
// It has to deal with overlapping situations.
// See https://github.com/w3c/csswg-drafts/issues/4483 for details.

// Return true if rect |a| is below |b|. False otherwise.
// For overlapping rects, |a| is considered to be below |b|,
// if the top edge of |a| is below the top edge of |b|.
static inline bool Below(const PhysicalRect& a, const PhysicalRect& b) {
  return a.Y() >= b.Bottom() || (a.Y() > b.Y() && a.IntersectsInclusively(b));
}

// Return true if rect |a| is above |b|. False otherwise.
// For overlapping rects, |a| is considered to be above |b|,
// if the bottom edge of |a| is above the bottom edge of |b|.
static inline bool Above(const PhysicalRect& a, const PhysicalRect& b) {
  return a.Bottom() <= b.Y() ||
         (a.Bottom() < b.Bottom() && a.IntersectsInclusively(b));
}

// Return true if rect |a| is on the right of |b|. False otherwise.
// For overlapping rects, |a| is considered to be on the right of |b|,
// if the left edge of |a| is on the right of the left edge of |b|.
static inline bool RightOf(const PhysicalRect& a, const PhysicalRect& b) {
  return a.X() >= b.Right() || (a.X() > b.X() && a.IntersectsInclusively(b));
}

// Return true if rect |a| is on the left of |b|. False otherwise.
// For overlapping rects, |a| is considered to be on the left of |b|,
// if the right edge of |a| is on the left of the right edge of |b|.
static inline bool LeftOf(const PhysicalRect& a, const PhysicalRect& b) {
  return a.Right() <= b.X() ||
         (a.Right() < b.Right() && a.IntersectsInclusively(b));
}

static bool IsRectInDirection(SpatialNavigationDirection direction,
                              const PhysicalRect& cur_rect,
                              const PhysicalRect& target_rect) {
  if (target_rect.Contains(cur_rect)) {
    // When leaving an "insider", don't focus its underlying container box.
    // Go directly to the outside world. This avoids focus from being trapped
    // inside a container.
    return false;
  } else if (cur_rect.Contains(target_rect)) {
    // Treat "insider" as rect in direction
    return true;
  }

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return LeftOf(target_rect, cur_rect);
    case SpatialNavigationDirection::kRight:
      return RightOf(target_rect, cur_rect);
    case SpatialNavigationDirection::kUp:
      return Above(target_rect, cur_rect);
    case SpatialNavigationDirection::kDown:
      return Below(target_rect, cur_rect);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

int LineBoxes(const LayoutObject& layout_object) {
  if (!layout_object.IsInline() || layout_object.IsAtomicInlineLevel())
    return 1;

  // If it has empty quads, it's most likely not a line broken ("fragmented")
  // text. <a><div></div></a> has for example one empty rect.
  Vector<gfx::QuadF> quads;
  layout_object.AbsoluteQuads(quads);
  for (const gfx::QuadF& quad : quads) {
    if (quad.BoundingBox().IsEmpty())
      return 1;
  }

  return quads.size();
}

bool IsFragmentedInline(const LayoutObject& layout_object) {
  return LineBoxes(layout_object) > 1;
}

gfx::RectF RectInViewport(const Node& node) {
  LocalFrameView* frame_view = node.GetDocument().View();
  if (!frame_view)
    return gfx::RectF();

  DCHECK(!frame_view->NeedsLayout());

  LayoutObject* object = node.GetLayoutObject();
  if (!object)
    return gfx::RectF();

  PhysicalRect rect_in_root_frame = NodeRectInRootFrame(&node);

  // Convert to the visual viewport which will account for pinch zoom.
  VisualViewport& visual_viewport =
      object->GetDocument().GetPage()->GetVisualViewport();
  gfx::RectF rect_in_viewport =
      visual_viewport.RootFrameToViewport(gfx::RectF(rect_in_root_frame));

  // RootFrameToViewport doesn't clip so manually apply the viewport clip here.
  gfx::RectF viewport_rect(gfx::SizeF(visual_viewport.Size()));
  rect_in_viewport.Intersect(viewport_rect);

  return rect_in_viewport;
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
  DCHECK(node);
  return RectInViewport(*node).IsEmpty();
}

ScrollableArea* ScrollableAreaFor(const Node* node) {
  if (node->IsDocumentNode()) {
    LocalFrameView* view = node->GetDocument().View();
    if (!view)
      return nullptr;

    return view->GetScrollableArea();
  }

  LayoutObject* object = node->GetLayoutObject();
  if (!object || !object->IsBox())
    return nullptr;

  return To<LayoutBox>(object)->GetScrollableArea();
}

bool IsUnobscured(const FocusCandidate& candidate) {
  DCHECK(candidate.visible_node);

  const LocalFrame* local_main_frame = DynamicTo<LocalFrame>(
      candidate.visible_node->GetDocument().GetPage()->MainFrame());
  if (!local_main_frame)
    return false;

  PhysicalRect viewport_rect(
      local_main_frame->GetPage()->GetVisualViewport().VisibleContentRect());
  PhysicalRect interesting_rect =
      Intersection(candidate.rect_in_root_frame, viewport_rect);

  if (interesting_rect.IsEmpty())
    return false;

  HitTestLocation location(interesting_rect);
  HitTestResult result =
      local_main_frame->GetEventHandler().HitTestResultAtLocation(
          location, HitTestRequest::kReadOnly | HitTestRequest::kListBased |
                        HitTestRequest::kIgnoreZeroOpacityObjects |
                        HitTestRequest::kAllowChildFrameContent);

  const HitTestResult::NodeSet& nodes = result.ListBasedTestResult();
  for (const auto& hit_node : base::Reversed(nodes)) {
    if (candidate.visible_node->ContainsIncludingHostElements(*hit_node))
      return true;

    if (FrameOwnerElement(candidate) &&
        FrameOwnerElement(candidate)
            ->contentDocument()
            ->ContainsIncludingHostElements(*hit_node))
      return true;
  }

  return false;
}

bool HasRemoteFrame(const Node* node) {
  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node);
  if (!frame_owner_element)
    return false;

  return frame_owner_element->ContentFrame() &&
         frame_owner_element->ContentFrame()->IsRemoteFrame();
}

bool ScrollInDirection(Node* container, SpatialNavigationDirection direction) {
  DCHECK(container);

  if (!CanScrollInDirection(container, direction))
    return false;

  int dx = 0;
  int dy = 0;
  int pixels_per_line_step =
      ScrollableArea::PixelsPerLineStep(container->GetDocument().GetFrame());
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      dx = -pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kRight:
      // TODO(bokan, https://crbug.com/952326): Fix this DCHECK.
      //  DCHECK_GT(container->GetLayoutBox()->ScrollWidth(),
      //            container->GetLayoutBoxForScrolling()
      //                    ->GetScrollableArea()
      //                    ->ScrollPosition()
      //                    .X() +
      //                container->GetLayoutBox()->ClientWidth());
      dx = pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kUp:
      dy = -pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kDown:
      // TODO(bokan, https://crbug.com/952326): Fix this DCHECK.
      //  DCHECK_GT(container->GetLayoutBox()->ScrollHeight(),
      //            container->GetLayoutBoxForScrolling()
      //                    ->GetScrollableArea()
      //                    ->ScrollPosition()
      //                    .Y() +
      //                container->GetLayoutBox()->ClientHeight());
      dy = pixels_per_line_step;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  // TODO(crbug.com/914775): Use UserScroll() instead. UserScroll() does a
  // smooth, animated scroll which might make it easier for users to understand
  // spatnav's moves. Another advantage of using ScrollableArea::UserScroll() is
  // that it returns a ScrollResult so we don't need to call
  // CanScrollInDirection(). Regular arrow-key scrolling (without
  // --enable-spatial-navigation) already uses smooth scrolling by default.
  ScrollableArea* scroller = ScrollableAreaFor(container);
  if (!scroller)
    return false;

  scroller->ScrollBy(ScrollOffset(dx, dy), mojom::blink::ScrollType::kUser);
  return true;
}

bool IsScrollableNode(const Node* node) {
  if (!node)
    return false;

  if (node->IsDocumentNode())
    return true;

  if (auto* box = DynamicTo<LayoutBox>(node->GetLayoutObject())) {
    return box->IsUserScrollable();
  }
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

  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node);
  return (frame_owner_element && frame_owner_element->ContentFrame()) ||
         IsScrollableNode(node);
}

bool CanScrollInDirection(const Node* container,
                          SpatialNavigationDirection direction) {
  DCHECK(container);
  if (auto* document = DynamicTo<Document>(container))
    return CanScrollInDirection(document->GetFrame(), direction);

  if (!IsScrollableNode(container))
    return false;

  const Element* container_element = DynamicTo<Element>(container);
  if (!container_element)
    return false;
  LayoutBox* box = container_element->GetLayoutBoxForScrolling();
  if (!box)
    return false;
  auto* scrollable_area = box->GetScrollableArea();
  if (!scrollable_area)
    return false;

  DCHECK(container->GetLayoutObject());
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              scrollable_area->GetScrollOffset().x() >
                  scrollable_area->MinimumScrollOffset().x());
    case SpatialNavigationDirection::kUp:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              scrollable_area->GetScrollOffset().y() >
                  scrollable_area->MinimumScrollOffset().y());
    case SpatialNavigationDirection::kRight:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              scrollable_area->GetScrollOffset().x() <
                  scrollable_area->MaximumScrollOffset().x());
    case SpatialNavigationDirection::kDown:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              scrollable_area->GetScrollOffset().y() <
                  scrollable_area->MaximumScrollOffset().y());
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool CanScrollInDirection(const LocalFrame* frame,
                          SpatialNavigationDirection direction) {
  if (!frame->View())
    return false;
  LayoutView* layoutView = frame->ContentLayoutObject();
  if (!layoutView)
    return false;
  mojom::blink::ScrollbarMode vertical_mode;
  mojom::blink::ScrollbarMode horizontal_mode;
  layoutView->CalculateScrollbarModes(horizontal_mode, vertical_mode);
  if ((direction == SpatialNavigationDirection::kLeft ||
       direction == SpatialNavigationDirection::kRight) &&
      mojom::blink::ScrollbarMode::kAlwaysOff == horizontal_mode)
    return false;
  if ((direction == SpatialNavigationDirection::kUp ||
       direction == SpatialNavigationDirection::kDown) &&
      mojom::blink::ScrollbarMode::kAlwaysOff == vertical_mode)
    return false;
  ScrollableArea* scrollable_area = frame->View()->GetScrollableArea();
  gfx::Size size = scrollable_area->ContentsSize();
  gfx::Vector2d offset = scrollable_area->ScrollOffsetInt();
  PhysicalRect rect(scrollable_area->VisibleContentRect(kIncludeScrollbars));

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return offset.x() > 0;
    case SpatialNavigationDirection::kUp:
      return offset.y() > 0;
    case SpatialNavigationDirection::kRight:
      return rect.Width() + offset.x() < size.width();
    case SpatialNavigationDirection::kDown:
      return rect.Height() + offset.y() < size.height();
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

PhysicalRect NodeRectInRootFrame(const Node* node) {
  DCHECK(node);
  DCHECK(node->GetLayoutObject());
  DCHECK(!node->GetDocument().View()->NeedsLayout());

  LayoutObject* object = node->GetLayoutObject();

  PhysicalRect rect = PhysicalRect::EnclosingRect(
      object->LocalBoundingBoxRectForAccessibility());

  // Inset the bounding box by the border.
  // TODO(bokan): As far as I can tell, this is to work around empty iframes
  // that have a border. It's unclear if that's still useful.
  rect.ContractEdges(LayoutUnit(object->StyleRef().BorderTopWidth()),
                     LayoutUnit(object->StyleRef().BorderRightWidth()),
                     LayoutUnit(object->StyleRef().BorderBottomWidth()),
                     LayoutUnit(object->StyleRef().BorderLeftWidth()));

  object->MapToVisualRectInAncestorSpace(/*ancestor=*/nullptr, rect);
  return rect;
}

// This method calculates the exit_point from the starting_rect and the
// entry_point into the candidate rect, and returns a pair of the entry_point
// and the exit_point.  The line between those 2 points is the closest
// distance between the 2 rects.  Takes care of overlapping rects, defining
// points so that the distance between them is zero where necessary.
std::pair<PhysicalOffset, PhysicalOffset> EntryAndExitPointsForDirection(
    SpatialNavigationDirection direction,
    const PhysicalRect& starting_rect,
    const PhysicalRect& potential_rect) {
  PhysicalOffset exit_point;
  PhysicalOffset entry_point;
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      exit_point.left = starting_rect.X();
      entry_point.left = std::min(potential_rect.Right(), starting_rect.X());
      break;
    case SpatialNavigationDirection::kUp:
      exit_point.top = starting_rect.Y();
      entry_point.top = std::min(potential_rect.Bottom(), starting_rect.Y());
      break;
    case SpatialNavigationDirection::kRight:
      exit_point.left = starting_rect.Right();
      entry_point.left = std::max(potential_rect.X(), starting_rect.Right());
      break;
    case SpatialNavigationDirection::kDown:
      exit_point.top = starting_rect.Bottom();
      entry_point.top = std::max(potential_rect.Y(), starting_rect.Bottom());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      if (Below(starting_rect, potential_rect)) {
        exit_point.top = starting_rect.Y();
        entry_point.top = std::min(potential_rect.Bottom(), starting_rect.Y());
      } else if (Below(potential_rect, starting_rect)) {
        exit_point.top = starting_rect.Bottom();
        entry_point.top = std::max(potential_rect.Y(), starting_rect.Bottom());
      } else {
        exit_point.top = std::max(starting_rect.Y(), potential_rect.Y());
        entry_point.top = exit_point.top;
      }
      break;
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      if (RightOf(starting_rect, potential_rect)) {
        exit_point.left = starting_rect.X();
        entry_point.left = std::min(potential_rect.Right(), starting_rect.X());
      } else if (RightOf(potential_rect, starting_rect)) {
        exit_point.left = starting_rect.Right();
        entry_point.left = std::max(potential_rect.X(), starting_rect.Right());
      } else {
        exit_point.left = std::max(starting_rect.X(), potential_rect.X());
        entry_point.left = exit_point.left;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return {entry_point, exit_point};
}

double ProjectedOverlap(SpatialNavigationDirection direction,
                        PhysicalRect current,
                        PhysicalRect candidate) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      current.SetWidth(LayoutUnit(1));
      candidate.SetX(current.X());
      current.Intersect(candidate);
      return current.Height();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      current.SetHeight(LayoutUnit(1));
      candidate.SetY(current.Y());
      current.Intersect(candidate);
      return current.Width();
    default:
      NOTREACHED_IN_MIGRATION();
      return kMaxDistance;
  }
}

double Alignment(SpatialNavigationDirection direction,
                 PhysicalRect current,
                 PhysicalRect candidate) {
  // The formula and constants for "alignment" are experimental and
  // come from https://drafts.csswg.org/css-nav-1/#heuristics.
  const int kAlignWeight = 5;

  double projected_overlap = ProjectedOverlap(direction, current, candidate);
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      return (kAlignWeight * projected_overlap) / current.Height();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      return (kAlignWeight * projected_overlap) / current.Width();
    default:
      NOTREACHED_IN_MIGRATION();
      return kMaxDistance;
  }
}

bool BothOnTopmostPaintLayerInStackingContext(
    const FocusCandidate& current_interest,
    const FocusCandidate& candidate) {
  if (!current_interest.visible_node)
    return false;

  const LayoutObject* origin = current_interest.visible_node->GetLayoutObject();
  const PaintLayer* focused_layer = origin->PaintingLayer();
  if (!focused_layer || focused_layer->IsRootLayer())
    return false;

  const LayoutObject* next = candidate.visible_node->GetLayoutObject();
  const PaintLayer* candidate_layer = next->PaintingLayer();
  if (focused_layer != candidate_layer)
    return false;

  return !candidate_layer->HasVisibleSelfPaintingDescendant();
}

double ComputeDistanceDataForNode(SpatialNavigationDirection direction,
                                  const FocusCandidate& current_interest,
                                  const FocusCandidate& candidate) {
  double distance = 0.0;
  PhysicalRect node_rect = candidate.rect_in_root_frame;
  PhysicalRect current_rect = current_interest.rect_in_root_frame;
  if (!IsRectInDirection(direction, current_rect, node_rect)) {
    return kMaxDistance;
  }

  if (BothOnTopmostPaintLayerInStackingContext(current_interest, candidate)) {
    // Prioritize "popup candidates" over other candidates by giving them a
    // negative, < 0, distance number.
    distance = kPriorityClassA;
  } else if (current_rect.IntersectsInclusively(node_rect)) {
    // We prioritize intersecting candidates, candidates that overlap
    // the current focus rect, by giving them a negative, < 0, distance
    // number. https://drafts.csswg.org/css-nav-1/#select-the-best-candidate
    distance = kPriorityClassB;

    // For intersecting candidates we cannot measure the distance from the
    // outer box. Instead, we measure distance _from_ the focused container's
    // rect's "opposite edge" in the navigated direction, just like we do when
    // we look for candidates inside a focused scroll container.
    current_rect = OppositeEdge(direction, current_rect);

    // This candidate overlaps the current focus rect so we can omit the
    // overlap term of the equation. An "intersecting candidate" will always
    // win against an "outsider".
  }

  const auto [entry_point, exit_point] =
      EntryAndExitPointsForDirection(direction, current_rect, node_rect);

  LayoutUnit x_axis = (exit_point.left - entry_point.left).Abs();
  LayoutUnit y_axis = (exit_point.top - entry_point.top).Abs();
  double euclidian_distance =
      sqrt((x_axis * x_axis + y_axis * y_axis).ToDouble());
  distance += euclidian_distance;

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
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      navigation_axis_distance = x_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Height() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (y_axis + orthogonal_bias) * kOrthogonalWeightForLeftRight;
      break;
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      navigation_axis_distance = y_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Width() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (x_axis + orthogonal_bias) * kOrthogonalWeightForUpDown;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return kMaxDistance;
  }

  // We try to formalize this distance calculation at
  // https://drafts.csswg.org/css-nav-1/.
  distance += weighted_orthogonal_axis_distance.ToDouble() +
              navigation_axis_distance.ToDouble();
  return distance - Alignment(direction, current_rect, node_rect);
}

// Returns a thin rectangle that represents one of |box|'s edges.
// To not intersect elements that are positioned inside |box|, we add one
// LayoutUnit of margin that puts the returned slice "just outside" |box|.
PhysicalRect OppositeEdge(SpatialNavigationDirection side,
                          const PhysicalRect& box,
                          LayoutUnit thickness) {
  PhysicalRect thin_rect = box;
  switch (side) {
    case SpatialNavigationDirection::kLeft:
      thin_rect.SetX(thin_rect.Right() - thickness);
      thin_rect.SetWidth(thickness);
      thin_rect.offset.left += 1;
      break;
    case SpatialNavigationDirection::kRight:
      thin_rect.SetWidth(thickness);
      thin_rect.offset.left -= 1;
      break;
    case SpatialNavigationDirection::kDown:
      thin_rect.SetHeight(thickness);
      thin_rect.offset.top -= 1;
      break;
    case SpatialNavigationDirection::kUp:
      thin_rect.SetY(thin_rect.Bottom() - thickness);
      thin_rect.SetHeight(thickness);
      thin_rect.offset.top += 1;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return thin_rect;
}

PhysicalRect StartEdgeForAreaElement(const HTMLAreaElement& area,
                                     SpatialNavigationDirection direction) {
  DCHECK(area.ImageElement());
  // Area elements tend to overlap more than other focusable elements. We
  // flatten the rect of the area elements to minimize the effect of overlapping
  // areas.
  PhysicalRect rect = OppositeEdge(
      direction,
      area.GetDocument().GetFrame()->View()->ConvertToRootFrame(
          area.ComputeAbsoluteRect(area.ImageElement()->GetLayoutObject())),
      LayoutUnit(kFudgeFactor) /* snav-imagemap-overlapped-areas.html */);
  return rect;
}

HTMLFrameOwnerElement* FrameOwnerElement(const FocusCandidate& candidate) {
  return DynamicTo<HTMLFrameOwnerElement>(candidate.visible_node);
}

// The visual viewport's rect (given in the root frame's coordinate space).
PhysicalRect RootViewport(const LocalFrame* current_frame) {
  return PhysicalRect::EnclosingRect(
      current_frame->GetPage()->GetVisualViewport().VisibleRect());
}

// Ignores fragments that are completely offscreen.
// Returns the first one that is not offscreen, in the given iterator range.
template <class Iterator>
PhysicalRect FirstVisibleFragment(const PhysicalRect& visibility,
                                  Iterator fragment,
                                  Iterator end) {
  while (fragment != end) {
    PhysicalRect physical_fragment =
        PhysicalRect::EnclosingRect(fragment->BoundingBox());
    physical_fragment.Intersect(visibility);
    if (!physical_fragment.IsEmpty())
      return physical_fragment;
    ++fragment;
  }
  return visibility;
}

LayoutUnit GetLogicalHeight(const PhysicalRect& rect,
                            const LayoutObject& layout_object) {
  if (layout_object.IsHorizontalWritingMode())
    return rect.Height();
  else
    return rect.Width();
}

void SetLogicalHeight(PhysicalRect& rect,
                      const LayoutObject& layout_object,
                      LayoutUnit height) {
  if (layout_object.IsHorizontalWritingMode())
    rect.SetHeight(height);
  else
    rect.SetWidth(height);
}

LayoutUnit TallestInlineAtomicChild(const LayoutObject& layout_object) {
  LayoutUnit max_child_size(0);

  if (!layout_object.IsLayoutInline())
    return max_child_size;

  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    if (child->IsAtomicInlineLevel()) {
      max_child_size =
          std::max(To<LayoutBox>(child)->LogicalHeight(), max_child_size);
    }
  }

  return max_child_size;
}

//   "Although margins, borders, and padding of non-replaced elements do not
//    enter into the line box calculation, they are still rendered around
//    inline boxes. This means that if the height specified by line-height is
//    less than the content height of contained boxes, backgrounds and colors
//    of padding and borders may "bleed" into adjoining line boxes". [1]
// [1] https://drafts.csswg.org/css2/#leading
// [2] https://drafts.csswg.org/css2/#line-box
// [3] https://drafts.csswg.org/css2/#atomic-inline-level-boxes
//
// If an inline box is "bleeding", ShrinkInlineBoxToLineBox shrinks its
// rect to the size of of its "line box" [2]. We need to do so because
// "bleeding" can make links intersect vertically. We need to avoid that
// overlap because it could make links on the same line (to the left or right)
// unreachable as SpatNav's distance formula favors intersecting rects (on the
// line below or above).
PhysicalRect ShrinkInlineBoxToLineBox(const LayoutObject& layout_object,
                                      PhysicalRect node_rect,
                                      int line_boxes) {
  if (!layout_object.IsInline() || layout_object.IsLayoutReplaced() ||
      layout_object.IsButtonOrInputButton()) {
    return node_rect;
  }

  // If actual line-height is bigger than the inline box, we shouldn't change
  // anything. This is, for example, needed to not break
  // snav-stay-in-overflow-div.html where the link's inline box doesn't fill
  // the entire line box vertically.
  LayoutUnit line_height = layout_object.StyleRef().ComputedLineHeightAsFixed();
  LayoutUnit current_height = GetLogicalHeight(node_rect, layout_object);
  if (line_height >= current_height)
    return node_rect;

  // Handle focusables like <a><img><a> (a LayoutInline that carries atomic
  // inline boxes [3]). Despite a small line-height on the <a>, <a>'s line box
  // will still fit the <img>.
  line_height = std::max(TallestInlineAtomicChild(layout_object), line_height);
  if (line_height >= current_height)
    return node_rect;

  // Cap the box at its line height to avoid overlapping inline links.
  // Links can overlap vertically when CSS line-height < font-size, see
  // snav-line-height_less_font-size.html.
  line_boxes = line_boxes == -1 ? LineBoxes(layout_object) : line_boxes;
  line_height = line_height * line_boxes;
  if (line_height >= current_height)
    return node_rect;
  SetLogicalHeight(node_rect, layout_object, line_height);
  return node_rect;
}

// TODO(crbug.com/1131419): Add tests and support for other writing-modes.
PhysicalRect SearchOriginFragment(const PhysicalRect& visible_part,
                                  const LayoutObject& fragmented,
                                  const SpatialNavigationDirection direction) {
  // For accuracy, use the first visible fragment (not the fragmented element's
  // entire bounding rect which is a union of all fragments) as search origin.
  Vector<gfx::QuadF> fragments;
  fragmented.AbsoluteQuads(
      fragments, kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kDown:
      // Search from the topmost fragment.
      return FirstVisibleFragment(visible_part, fragments.begin(),
                                  fragments.end());
    case SpatialNavigationDirection::kRight:
    case SpatialNavigationDirection::kUp:
      // Search from the bottommost fragment.
      return FirstVisibleFragment(visible_part, fragments.rbegin(),
                                  fragments.rend());
    case SpatialNavigationDirection::kNone:
      break;
      // Nothing to do.
  }
  return visible_part;
}

// Spatnav uses this rectangle to measure distances to focus candidates.
// The search origin is either activeElement F itself, if it's being at least
// partially visible, or else, its first [partially] visible scroller. If both
// F and its enclosing scroller are completely off-screen, we recurse to the
// scroller’s scroller ... all the way up until the root frame's document.
// The root frame's document is a good base case because it's, per definition,
// a visible scrollable area.
PhysicalRect SearchOrigin(const PhysicalRect& viewport_rect_of_root_frame,
                          Node* focus_node,
                          const SpatialNavigationDirection direction) {
  if (!focus_node) {
    // Search from one of the visual viewport's edges towards the navigated
    // direction. For example, UP makes spatnav search upwards, starting at the
    // visual viewport's bottom.
    return OppositeEdge(direction, viewport_rect_of_root_frame);
  }

  auto* area_element = DynamicTo<HTMLAreaElement>(focus_node);
  if (area_element)
    focus_node = area_element->ImageElement();

  if (!IsOffscreen(focus_node)) {
    if (area_element)
      return StartEdgeForAreaElement(*area_element, direction);

    PhysicalRect box_in_root_frame = NodeRectInRootFrame(focus_node);
    PhysicalRect visible_part =
        Intersection(box_in_root_frame, viewport_rect_of_root_frame);

    const LayoutObject* const layout_object = focus_node->GetLayoutObject();
    if (IsFragmentedInline(*layout_object)) {
      visible_part =
          SearchOriginFragment(visible_part, *layout_object, direction);
    }

    // Remove any overlap with line boxes *below* the search origin.
    // The search origin is always only one line (because if |focus_node| is
    // line broken, SearchOriginFragment picks the first or last line's box).
    visible_part = ShrinkInlineBoxToLineBox(*layout_object, visible_part, 1);

    return visible_part;
  }

  Node* container = ScrollableAreaOrDocumentOf(focus_node);
  while (container) {
    if (!IsOffscreen(container)) {
      // The first scroller that encloses focus and is [partially] visible.
      PhysicalRect box_in_root_frame = NodeRectInRootFrame(container);
      return OppositeEdge(direction, Intersection(box_in_root_frame,
                                                  viewport_rect_of_root_frame));
    }
    container = ScrollableAreaOrDocumentOf(container);
  }
  return OppositeEdge(direction, viewport_rect_of_root_frame);
}

}  // namespace blink
