/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/page/touch_adjustment.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace touch_adjustment {

const float kZeroTolerance = 1e-6f;
// The touch adjustment range (diameters) in dip, using same as the value in
// gesture_configuration_android.cc
constexpr LayoutUnit kMaxAdjustmentSizeDip(32);
constexpr LayoutUnit kMinAdjustmentSizeDip(20);

// Class for remembering absolute quads of a target node and what node they
// represent.
class SubtargetGeometry {
  DISALLOW_NEW();

 public:
  SubtargetGeometry(Node* node, const gfx::QuadF& quad)
      : node_(node), quad_(quad) {}
  void Trace(Visitor* visitor) const { visitor->Trace(node_); }

  Node* GetNode() const { return node_.Get(); }
  gfx::QuadF Quad() const { return quad_; }
  gfx::Rect BoundingBox() const {
    return gfx::ToEnclosingRect(quad_.BoundingBox());
  }

 private:
  Member<Node> node_;
  gfx::QuadF quad_;
};

}  // namespace touch_adjustment

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::touch_adjustment::SubtargetGeometry)

namespace blink {

namespace touch_adjustment {

typedef HeapVector<SubtargetGeometry> SubtargetGeometryList;
typedef bool (*NodeFilter)(Node*);
typedef void (*AppendSubtargetsForNode)(Node*, SubtargetGeometryList&);
typedef float (*DistanceFunction)(const gfx::Point&,
                                  const gfx::Rect&,
                                  const SubtargetGeometry&);

// Takes non-const |Node*| because |Node::WillRespondToMouseClickEvents()| is
// non-const.
bool NodeRespondsToTapGesture(Node* node) {
  if (node->WillRespondToMouseClickEvents() ||
      node->WillRespondToMouseMoveEvents())
    return true;
  if (auto* element = DynamicTo<Element>(node)) {
    // Tapping on a text field or other focusable item should trigger
    // adjustment, except that iframe elements are hard-coded to support focus
    // but the effect is often invisible so they should be excluded.
    if (element->IsMouseFocusable() && !IsA<HTMLIFrameElement>(element)) {
      return true;
    }
    // Accept nodes that has a CSS effect when touched.
    if (element->ChildrenOrSiblingsAffectedByActive() ||
        element->ChildrenOrSiblingsAffectedByHover())
      return true;
  }
  if (const ComputedStyle* computed_style = node->GetComputedStyle()) {
    if (computed_style->AffectedByActive() || computed_style->AffectedByHover())
      return true;
  }
  return false;
}

bool NodeIsZoomTarget(Node* node) {
  if (node->IsTextNode() || node->IsShadowRoot())
    return false;

  DCHECK(node->GetLayoutObject());
  return node->GetLayoutObject()->IsBox();
}

bool ProvidesContextMenuItems(Node* node) {
  // This function tries to match the nodes that receive special context-menu
  // items in ContextMenuController::ShowContextMenu(), and should be kept up
  // to date with those.
  DCHECK(node->GetLayoutObject() || node->IsShadowRoot());
  if (!node->GetLayoutObject())
    return false;
  node->GetDocument().UpdateStyleAndLayoutTree();
  if (IsEditable(*node))
    return true;
  if (node->IsLink())
    return true;
  if (node->GetLayoutObject()->IsImage())
    return true;
  if (node->GetLayoutObject()->IsMedia())
    return true;
  if (node->GetLayoutObject()->IsSVGImage()) {
    return true;
  }
  if (node->GetLayoutObject()->CanBeSelectionLeaf()) {
    // If the context menu gesture will trigger a selection all selectable nodes
    // are valid targets.
    if (node->GetLayoutObject()
            ->GetFrame()
            ->GetEditor()
            .Behavior()
            .ShouldSelectOnContextualMenuClick())
      return true;
    // Only the selected part of the layoutObject is a valid target, but this
    // will be corrected in appendContextSubtargetsForNode.
    if (node->GetLayoutObject()->IsSelected())
      return true;
  }
  return false;
}

bool NodeRespondsToTapOrMove(Node* node) {
  // This method considers nodes from NodeRespondsToTapGesture, those where pan
  // touch action is disabled, and ones that are stylus writable. We do this to
  // avoid adjusting the pointer position on drawable area or slidable control
  // to the nearby writable input node.
  node->GetDocument().UpdateStyleAndLayoutTree();

  if (NodeRespondsToTapGesture(node))
    return true;

  TouchAction effective_touch_action =
      touch_action_util::ComputeEffectiveTouchAction(*node);

  if ((effective_touch_action & TouchAction::kPan) != TouchAction::kPan)
    return true;

  if ((effective_touch_action & TouchAction::kInternalNotWritable) !=
      TouchAction::kInternalNotWritable) {
    return true;
  }
  return false;
}

static inline void AppendQuadsToSubtargetList(
    Vector<gfx::QuadF>& quads,
    Node* node,
    SubtargetGeometryList& subtargets) {
  for (const auto& quad : quads) {
    subtargets.push_back(SubtargetGeometry(node, quad));
  }
}

static inline void AppendBasicSubtargetsForNode(
    Node* node,
    SubtargetGeometryList& subtargets) {
  // Node guaranteed to have layoutObject due to check in node filter.
  DCHECK(node->GetLayoutObject());

  Vector<gfx::QuadF> quads;
  node->GetLayoutObject()->AbsoluteQuads(quads);

  AppendQuadsToSubtargetList(quads, node, subtargets);
}

static inline void AppendContextSubtargetsForNode(
    Node* node,
    SubtargetGeometryList& subtargets) {
  // This is a variant of appendBasicSubtargetsForNode that adds special
  // subtargets for selected or auto-selectable parts of text nodes.
  DCHECK(node->GetLayoutObject());

  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return AppendBasicSubtargetsForNode(node, subtargets);

  LayoutText* text_layout_object = text_node->GetLayoutObject();

  if (text_layout_object->GetFrame()
          ->GetEditor()
          .Behavior()
          .ShouldSelectOnContextualMenuClick()) {
    // Make subtargets out of every word.
    String text_value = text_node->data();
    TextBreakIterator* word_iterator =
        WordBreakIterator(text_value, 0, text_value.length());
    int last_offset = word_iterator->first();
    if (last_offset == -1)
      return;
    int offset;
    while ((offset = word_iterator->next()) != -1) {
      if (IsWordTextBreak(word_iterator)) {
        Vector<gfx::QuadF> quads;
        text_layout_object->AbsoluteQuadsForRange(quads, last_offset, offset);
        AppendQuadsToSubtargetList(quads, text_node, subtargets);
      }
      last_offset = offset;
    }
  } else {
    if (!text_layout_object->IsSelected())
      return AppendBasicSubtargetsForNode(node, subtargets);
    const FrameSelection& frame_selection =
        text_layout_object->GetFrame()->Selection();
    const LayoutTextSelectionStatus& selection_status =
        frame_selection.ComputeLayoutSelectionStatus(*text_layout_object);
    // If selected, make subtargets out of only the selected part of the text.
    Vector<gfx::QuadF> quads;
    text_layout_object->AbsoluteQuadsForRange(quads, selection_status.start,
                                              selection_status.end);
    AppendQuadsToSubtargetList(quads, text_node, subtargets);
  }
}

static inline Node* ParentShadowHostOrOwner(const Node* node) {
  if (Node* ancestor = node->ParentOrShadowHostNode())
    return ancestor;
  if (auto* document = DynamicTo<Document>(node))
    return document->LocalOwner();
  return nullptr;
}

// Compiles a list of subtargets of all the relevant target nodes.
void CompileSubtargetList(const HeapVector<Member<Node>>& intersected_nodes,
                          SubtargetGeometryList& subtargets,
                          NodeFilter node_filter,
                          AppendSubtargetsForNode append_subtargets_for_node) {
  // Find candidates responding to tap gesture events in O(n) time.
  HeapHashMap<Member<Node>, Member<Node>> responder_map;
  HeapHashSet<Member<Node>> ancestors_to_responders_set;
  HeapVector<Member<Node>> candidates;
  HeapHashSet<Member<Node>> editable_ancestors;

  // A node matching the NodeFilter is called a responder. Candidate nodes must
  // either be a responder or have an ancestor that is a responder.  This
  // iteration tests all ancestors at most once by caching earlier results.
  for (unsigned i = 0; i < intersected_nodes.size(); ++i) {
    Node* node = intersected_nodes[i].Get();
    HeapVector<Member<Node>> visited_nodes;
    Node* responding_node = nullptr;
    for (Node* visited_node = node; visited_node;
         visited_node = visited_node->ParentOrShadowHostNode()) {
      // Check if we already have a result for a common ancestor from another
      // candidate.
      const auto it = responder_map.find(visited_node);
      if (it != responder_map.end()) {
        responding_node = it->value;
        break;
      }
      visited_nodes.push_back(visited_node);
      // Check if the node filter applies, which would mean we have found a
      // responding node.
      if (node_filter(visited_node)) {
        responding_node = visited_node;
        // Continue the iteration to collect the ancestors of the responder,
        // which we will need later.
        for (visited_node = ParentShadowHostOrOwner(visited_node); visited_node;
             visited_node = ParentShadowHostOrOwner(visited_node)) {
          HeapHashSet<Member<Node>>::AddResult add_result =
              ancestors_to_responders_set.insert(visited_node);
          if (!add_result.is_new_entry)
            break;
        }
        break;
      }
    }
    if (responding_node) {
      // Insert the detected responder for all the visited nodes.
      for (unsigned j = 0; j < visited_nodes.size(); j++)
        responder_map.insert(visited_nodes[j], responding_node);

      candidates.push_back(node);
    }
  }

  // We compile the list of component absolute quads instead of using the
  // bounding rect to be able to perform better hit-testing on inline links on
  // line-breaks.
  for (unsigned i = 0; i < candidates.size(); i++) {
    Node* candidate = candidates[i];

    // Skip nodes whose responders are ancestors of other responders. This gives
    // preference to the inner-most event-handlers. So that a link is always
    // preferred even when contained in an element that monitors all
    // click-events.
    Node* responding_node = responder_map.at(candidate);
    DCHECK(responding_node);
    if (ancestors_to_responders_set.Contains(responding_node))
      continue;

    // Consolidate bounds for editable content.
    if (editable_ancestors.Contains(candidate))
      continue;
    candidate->GetDocument().UpdateStyleAndLayoutTree();
    if (IsEditable(*candidate)) {
      Node* replacement = candidate;
      Node* parent = candidate->ParentOrShadowHostNode();

      // Ignore parents without layout objects.  E.g. editable elements with
      // display:contents.  https://crbug.com/1196872
      while (parent && IsEditable(*parent) && parent->GetLayoutObject()) {
        replacement = parent;
        if (editable_ancestors.Contains(replacement)) {
          replacement = nullptr;
          break;
        }
        editable_ancestors.insert(replacement);
        parent = parent->ParentOrShadowHostNode();
      }
      candidate = replacement;
    }
    if (candidate)
      append_subtargets_for_node(candidate, subtargets);
  }
}

// This returns quotient of the target area and its intersection with the touch
// area.  This will prioritize largest intersection and smallest area, while
// balancing the two against each other.
float ZoomableIntersectionQuotient(const gfx::Point& touch_hotspot,
                                   const gfx::Rect& touch_area,
                                   const SubtargetGeometry& subtarget) {
  gfx::Rect rect =
      subtarget.GetNode()->GetDocument().View()->ConvertToRootFrame(
          subtarget.BoundingBox());

  // Check the rectangle is meaningful zoom target. It should at least contain
  // the hotspot.
  if (!rect.Contains(touch_hotspot))
    return std::numeric_limits<float>::infinity();
  gfx::Rect intersection = rect;
  intersection.Intersect(touch_area);

  // Return the quotient of the intersection.
  return static_cast<float>(rect.size().Area64()) /
         static_cast<float>(intersection.size().Area64());
}

// Uses a hybrid of distance to adjust and intersect ratio, normalizing each
// score between 0 and 1 and combining them. The distance to adjust works best
// for disambiguating clicks on targets such as links, where the width may be
// significantly larger than the touch width.  Using area of overlap in such
// cases can lead to a bias towards shorter links. Conversely, percentage of
// overlap can provide strong confidence in tapping on a small target, where the
// overlap is often quite high, and works well for tightly packed controls.
float HybridDistanceFunction(const gfx::Point& touch_hotspot,
                             const gfx::Rect& touch_rect,
                             const SubtargetGeometry& subtarget) {
  gfx::RectF rect(subtarget.GetNode()->GetDocument().View()->ConvertToRootFrame(
      subtarget.BoundingBox()));
  float radius_squared =
      0.25f *
      gfx::Vector2dF(touch_rect.width(), touch_rect.height()).LengthSquared();
  gfx::PointF hotspot_f(touch_hotspot);
  float distance_to_adjust_score =
      (rect.ClosestPoint(hotspot_f) - hotspot_f).LengthSquared() /
      radius_squared;

  float max_overlap_width = std::min<float>(touch_rect.width(), rect.width());
  float max_overlap_height =
      std::min<float>(touch_rect.height(), rect.height());
  float max_overlap_area =
      std::max<float>(max_overlap_width * max_overlap_height, 1);
  rect.Intersect(gfx::RectF(touch_rect));
  float intersect_area = rect.size().GetArea();
  float intersection_score = 1 - intersect_area / max_overlap_area;

  float hybrid_score = intersection_score + distance_to_adjust_score;

  return hybrid_score;
}

gfx::PointF ConvertToRootFrame(LocalFrameView* view, gfx::PointF pt) {
  int x = static_cast<int>(pt.x() + 0.5f);
  int y = static_cast<int>(pt.y() + 0.5f);
  gfx::Point adjusted = view->ConvertToRootFrame(gfx::Point(x, y));
  return gfx::PointF(adjusted.x(), adjusted.y());
}

// Adjusts 'point' to the nearest point inside rect, and leaves it unchanged if
// already inside.
void AdjustPointToRect(gfx::PointF& point, const gfx::Rect& rect) {
  if (point.x() < rect.x()) {
    point.set_x(rect.x());
  } else if (point.x() >= rect.right()) {
    point.set_x(rect.right() - 1);
  }

  if (point.y() < rect.y()) {
    point.set_y(rect.y());
  } else if (point.y() >= rect.bottom()) {
    point.set_y(rect.bottom() - 1);
  }
}

bool SnapTo(const SubtargetGeometry& geom,
            const gfx::Point& touch_point,
            const gfx::Rect& touch_area,
            gfx::Point& snapped_point) {
  LocalFrameView* view = geom.GetNode()->GetDocument().View();
  gfx::QuadF quad = geom.Quad();

  if (quad.IsRectilinear()) {
    gfx::Rect bounds = view->ConvertToRootFrame(geom.BoundingBox());
    if (bounds.Contains(touch_point)) {
      snapped_point = touch_point;
      return true;
    }
    if (bounds.Intersects(touch_area)) {
      bounds.Intersect(touch_area);
      snapped_point = bounds.CenterPoint();
      return true;
    }
    return false;
  }

  // The following code tries to adjust the point to place inside a both the
  // touchArea and the non-rectilinear quad.
  // FIXME: This will return the point inside the touch area that is the closest
  // to the quad center, but does not guarantee that the point will be inside
  // the quad. Corner-cases exist where the quad will intersect but this will
  // fail to adjust the point to somewhere in the intersection.

  gfx::PointF p1 = ConvertToRootFrame(view, quad.p1());
  gfx::PointF p2 = ConvertToRootFrame(view, quad.p2());
  gfx::PointF p3 = ConvertToRootFrame(view, quad.p3());
  gfx::PointF p4 = ConvertToRootFrame(view, quad.p4());
  quad = gfx::QuadF(p1, p2, p3, p4);

  if (quad.Contains(gfx::PointF(touch_point))) {
    snapped_point = touch_point;
    return true;
  }

  // Pull point towards the center of the element.
  gfx::PointF center = quad.CenterPoint();

  AdjustPointToRect(center, touch_area);
  snapped_point = gfx::ToRoundedPoint(center);

  return quad.Contains(gfx::PointF(snapped_point));
}

// A generic function for finding the target node with the lowest distance
// metric. A distance metric here is the result of a distance-like function,
// that computes how well the touch hits the node.  Distance functions could for
// instance be distance squared or area of intersection.
bool FindNodeWithLowestDistanceMetric(Node*& adjusted_node,
                                      gfx::Point& adjusted_point,
                                      const gfx::Point& touch_hotspot,
                                      const gfx::Rect& touch_area,
                                      SubtargetGeometryList& subtargets,
                                      DistanceFunction distance_function) {
  adjusted_node = nullptr;
  float best_distance_metric = std::numeric_limits<float>::infinity();
  gfx::Point snapped_point;

  for (const auto& subtarget : subtargets) {
    Node* node = subtarget.GetNode();
    float distance_metric =
        distance_function(touch_hotspot, touch_area, subtarget);
    if (distance_metric < best_distance_metric) {
      if (SnapTo(subtarget, touch_hotspot, touch_area, snapped_point)) {
        adjusted_point = snapped_point;
        adjusted_node = node;
        best_distance_metric = distance_metric;
      }
    } else if (distance_metric - best_distance_metric < kZeroTolerance) {
      if (SnapTo(subtarget, touch_hotspot, touch_area, snapped_point)) {
        if (node->IsDescendantOf(adjusted_node)) {
          // Try to always return the inner-most element.
          adjusted_point = snapped_point;
          adjusted_node = node;
        }
      }
    }
  }

  // As for HitTestResult.innerNode, we skip over pseudo elements.
  if (adjusted_node && adjusted_node->IsPseudoElement() &&
      !adjusted_node->IsScrollMarkerPseudoElement()) {
    adjusted_node = adjusted_node->ParentOrShadowHostNode();
  }

  return adjusted_node != nullptr;
}

bool FindBestCandidate(Node*& adjusted_node,
                       gfx::Point& adjusted_point,
                       const gfx::Point& touch_hotspot,
                       const gfx::Rect& touch_area,
                       const HeapVector<Member<Node>>& nodes,
                       NodeFilter node_filter,
                       AppendSubtargetsForNode append_subtargets_for_node) {
  touch_adjustment::SubtargetGeometryList subtargets;
  touch_adjustment::CompileSubtargetList(nodes, subtargets, node_filter,
                                         append_subtargets_for_node);
  return touch_adjustment::FindNodeWithLowestDistanceMetric(
      adjusted_node, adjusted_point, touch_hotspot, touch_area, subtargets,
      touch_adjustment::HybridDistanceFunction);
}

}  // namespace touch_adjustment

bool FindBestTouchAdjustmentCandidate(
    TouchAdjustmentCandidateType candidate_type,
    Node*& candidate_node,
    gfx::Point& candidate_point,
    const gfx::Point& touch_hotspot,
    const gfx::Rect& touch_area,
    const HeapVector<Member<Node>>& nodes) {
  touch_adjustment::NodeFilter node_filter;
  touch_adjustment::AppendSubtargetsForNode append_subtargets_for_node;

  switch (candidate_type) {
    case TouchAdjustmentCandidateType::kClickable:
      node_filter = touch_adjustment::NodeRespondsToTapGesture;
      append_subtargets_for_node =
          touch_adjustment::AppendBasicSubtargetsForNode;
      break;
    case TouchAdjustmentCandidateType::kContextMenu:
      node_filter = touch_adjustment::ProvidesContextMenuItems;
      append_subtargets_for_node =
          touch_adjustment::AppendContextSubtargetsForNode;
      break;
    case TouchAdjustmentCandidateType::kStylusWritable:
      node_filter = touch_adjustment::NodeRespondsToTapOrMove;
      append_subtargets_for_node =
          touch_adjustment::AppendBasicSubtargetsForNode;
      break;
  }
  return FindBestCandidate(candidate_node, candidate_point, touch_hotspot,
                           touch_area, nodes, node_filter,
                           append_subtargets_for_node);
}

PhysicalSize GetHitTestRectForAdjustment(LocalFrame& frame,
                                         const PhysicalSize& touch_area) {
  ChromeClient& chrome_client = frame.GetChromeClient();
  float device_scale_factor =
      chrome_client.GetScreenInfo(frame).device_scale_factor;
  if (frame.GetPage()->InspectorDeviceScaleFactorOverride() != 1) {
    device_scale_factor = 1;
  }

  float page_scale_factor = frame.GetPage()->PageScaleFactor();
  const PhysicalSize max_size_in_dip(touch_adjustment::kMaxAdjustmentSizeDip,
                                     touch_adjustment::kMaxAdjustmentSizeDip);

  const PhysicalSize min_size_in_dip(touch_adjustment::kMinAdjustmentSizeDip,
                                     touch_adjustment::kMinAdjustmentSizeDip);
  // (when use-zoom-for-dsf enabled) touch_area is in physical pixel scaled,
  // max_size_in_dip should be converted to physical pixel and scale too.
  return touch_area
      .ShrunkTo(max_size_in_dip * (device_scale_factor / page_scale_factor))
      .ExpandedTo(min_size_in_dip * (device_scale_factor / page_scale_factor));
}

}  // namespace blink
