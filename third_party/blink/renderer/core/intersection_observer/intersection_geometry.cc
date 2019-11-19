// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

namespace {

// Return true if ancestor is in the containing block chain above descendant.
bool IsContainingBlockChainDescendant(const LayoutObject* descendant,
                                      const LayoutObject* ancestor) {
  if (!ancestor || !descendant)
    return false;
  LocalFrame* ancestor_frame = ancestor->GetDocument().GetFrame();
  LocalFrame* descendant_frame = descendant->GetDocument().GetFrame();
  if (ancestor_frame != descendant_frame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->ContainingBlock();
  return descendant;
}

// Convert a Length value to physical pixels.
LayoutUnit ComputeMargin(const Length& length,
                         LayoutUnit reference_length,
                         float zoom) {
  if (length.IsPercent()) {
    return LayoutUnit(static_cast<int>(reference_length.ToFloat() *
                                       length.Percent() / 100.0));
  }
  DCHECK(length.IsFixed());
  return LayoutUnit(length.Value() * zoom);
}

// Expand rect by the given margin values.
void ApplyRootMargin(PhysicalRect& rect,
                     const Vector<Length>& margin,
                     float zoom) {
  if (margin.IsEmpty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  LayoutRectOutsets outsets(ComputeMargin(margin[0], rect.Height(), zoom),
                            ComputeMargin(margin[1], rect.Width(), zoom),
                            ComputeMargin(margin[2], rect.Height(), zoom),
                            ComputeMargin(margin[3], rect.Width(), zoom));
  rect.Expand(outsets);
}

// Return the bounding box of target in target's own coordinate system
PhysicalRect InitializeTargetRect(const LayoutObject* target, unsigned flags) {
  if ((flags & IntersectionGeometry::kShouldUseReplacedContentRect) &&
      target->IsLayoutEmbeddedContent()) {
    return ToLayoutEmbeddedContent(target)->ReplacedContentRect();
  }
  if (target->IsBox())
    return PhysicalRect(ToLayoutBoxModelObject(target)->BorderBoundingBox());
  if (target->IsLayoutInline()) {
    return target->AbsoluteToLocalRect(
        PhysicalRect::EnclosingRect(target->AbsoluteBoundingBoxFloatRect()));
  }
  return ToLayoutText(target)->PhysicalLinesBoundingBox();
}

// Return the local frame root for a given object
LayoutView* LocalRootView(const LayoutObject& object) {
  const LocalFrame* frame = object.GetDocument().GetFrame();
  const LocalFrame* frame_root = frame ? &frame->LocalFrameRoot() : nullptr;
  return frame_root ? frame_root->ContentLayoutObject() : nullptr;
}

// Returns true if target has visual effects applied, or if rect, given in
// absolute coordinates, is overlapped by any content painted after target
//
//   https://w3c.github.io/IntersectionObserver/v2/#calculate-visibility-algo
bool ComputeIsVisible(const LayoutObject* target, const PhysicalRect& rect) {
  DCHECK(RuntimeEnabledFeatures::IntersectionObserverV2Enabled());
  if (target->GetDocument().GetFrame()->LocalFrameRoot().GetOcclusionState() !=
      FrameOcclusionState::kGuaranteedNotOccluded) {
    return false;
  }
  if (target->HasDistortingVisualEffects())
    return false;
  // TODO(layout-dev): This should hit-test the intersection rect, not the
  // target rect; it's not helpful to know that the portion of the target that
  // is clipped is also occluded.
  HitTestResult result(target->HitTestForOcclusion(rect));
  const Node* hit_node = result.InnerNode();
  if (!hit_node || hit_node == target->GetNode())
    return true;
  // TODO(layout-dev): This IsDescendantOf tree walk could be optimized by
  // stopping when hit_node's containing LayoutBlockFlow is reached.
  if (target->IsLayoutInline())
    return hit_node->IsDescendantOf(target->GetNode());
  return false;
}

// Returns the root intersect rect for the given root object, with the given
// margins applied, in the coordinate system of the root object.
//
//   https://w3c.github.io/IntersectionObserver/#intersectionobserver-root-intersection-rectangle
PhysicalRect InitializeRootRect(const LayoutObject* root,
                                const Vector<Length>& margin) {
  DCHECK(margin.IsEmpty() || margin.size() == 4);
  PhysicalRect result;
  if (root->IsLayoutView() && root->GetDocument().IsInMainFrame()) {
    // The main frame is a bit special as the scrolling viewport can differ in
    // size from the LayoutView itself. There's two situations this occurs in:
    // 1) The ForceZeroLayoutHeight quirk setting is used in Android WebView for
    // compatibility and sets the initial-containing-block's (a.k.a.
    // LayoutView) height to 0. Thus, we can't use its size for intersection
    // testing. Use the FrameView geometry instead.
    // 2) An element wider than the ICB can cause us to resize the FrameView so
    // we can zoom out to fit the entire element width.
    result = ToLayoutView(root)->OverflowClipRect(PhysicalOffset());
  } else if (root->IsBox() && root->HasOverflowClip()) {
    result = ToLayoutBox(root)->PhysicalContentBoxRect();
  } else {
    result = PhysicalRect(ToLayoutBoxModelObject(root)->BorderBoundingBox());
  }
  ApplyRootMargin(result, margin, root->StyleRef().EffectiveZoom());
  return result;
}

// Validates the given target element and returns its LayoutObject
LayoutObject* GetTargetLayoutObject(const Element& target_element) {
  if (!target_element.isConnected())
    return nullptr;
  LayoutObject* target = target_element.GetLayoutObject();
  if (!target || (!target->IsBoxModelObject() && !target->IsText()))
    return nullptr;
  // If the target is inside a locked subtree, it isn't ever visible.
  if (UNLIKELY(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
          target_element))) {
    return nullptr;
  }

  DCHECK(!target_element.GetDocument().View()->NeedsLayout());
  return target;
}

// If root_element is non-null, it is treated as the explicit root of an
// IntersectionObserver; if it is valid, its LayoutObject is returned.
// If root_element is null, returns the object to be used as the implicit root
// for a given target.
//
//   https://w3c.github.io/IntersectionObserver/#dom-intersectionobserver-root
LayoutObject* GetRootLayoutObjectForTarget(const Element* root_element,
                                           LayoutObject* target) {
  if (!target)
    return nullptr;
  if (root_element && !root_element->isConnected())
    return nullptr;
  LayoutObject* root =
      root_element ? root_element->GetLayoutObject() : LocalRootView(*target);
  if (!root)
    return nullptr;
  if (root_element && !IsContainingBlockChainDescendant(target, root))
    return nullptr;
  return root;
}

static const unsigned kConstructorFlagsMask =
    IntersectionGeometry::kShouldReportRootBounds |
    IntersectionGeometry::kShouldComputeVisibility |
    IntersectionGeometry::kShouldTrackFractionOfRoot |
    IntersectionGeometry::kShouldUseReplacedContentRect |
    IntersectionGeometry::kShouldConvertToCSSPixels;

}  // namespace

IntersectionGeometry::RootGeometry::RootGeometry(const LayoutObject* root,
                                                 const Vector<Length>& margin) {
  if (!root || !root->GetNode() || !root->GetNode()->isConnected() ||
      !root->IsBox())
    return;
  if (RuntimeEnabledFeatures::
          IntersectionObserverDocumentScrollingElementRootEnabled() &&
      root->GetNode() == root->GetDocument().scrollingElement()) {
    root = root->GetDocument().GetLayoutView();
  }
  zoom = root->StyleRef().EffectiveZoom();
  local_root_rect = InitializeRootRect(root, margin);
  TransformState transform_state(TransformState::kApplyTransformDirection);
  root->MapLocalToAncestor(nullptr, transform_state, 0);
  root_to_document_transform = transform_state.AccumulatedTransform();
}

IntersectionGeometry::IntersectionGeometry(const Element* root_element,
                                           const Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           unsigned flags)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  if (!root_element)
    flags_ |= kRootIsImplicit;
  LayoutObject* target = GetTargetLayoutObject(target_element);
  LayoutObject* root = GetRootLayoutObjectForTarget(root_element, target);
  if (!root || !target)
    return;
  RootGeometry root_geometry(root, root_margin);
  ComputeGeometry(root_geometry, root, target, thresholds);
}

IntersectionGeometry::IntersectionGeometry(const RootGeometry& root_geometry,
                                           const Element& explicit_root,
                                           const Element& target_element,
                                           const Vector<float>& thresholds,
                                           unsigned flags)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  LayoutObject* target = GetTargetLayoutObject(target_element);
  LayoutObject* root = explicit_root.GetLayoutObject();
  if (!IsContainingBlockChainDescendant(target, root))
    return;
  ComputeGeometry(root_geometry, root, target, thresholds);
}

IntersectionGeometry::~IntersectionGeometry() = default;

void IntersectionGeometry::ComputeGeometry(const RootGeometry& root_geometry,
                                           const LayoutObject* root,
                                           const LayoutObject* target,
                                           const Vector<float>& thresholds) {
  // Initially:
  //   target_rect_ is in target's coordinate system
  //   intersection_rect_ is in target's coordinate system
  //   root_rect_ is in root's coordinate system
  target_rect_ = InitializeTargetRect(target, flags_);
  intersection_rect_ = target_rect_;
  root_rect_ = root_geometry.local_root_rect;

  // This maps intersection_rect_ up to root's coordinate system
  bool does_intersect =
      ClipToRoot(root, target, root_rect_, intersection_rect_);

  // Map target_rect_ to absolute coordinates for target's document
  target_rect_ = target->LocalToAncestorRect(target_rect_, nullptr);
  if (does_intersect) {
    if (RootIsImplicit()) {
      // intersection_rect_ is in the coordinate system of the implicit root;
      // map it down the to absolute coordinates for the target's document.
      intersection_rect_ =
          target->GetDocument().GetLayoutView()->AbsoluteToLocalRect(
              intersection_rect_,
              kTraverseDocumentBoundaries | kApplyRemoteRootFrameOffset);
    } else {
      // intersection_rect_ is in root's coordinate system; map it up to
      // absolute coordinates for target's containing document (which is the
      // same as root's document).
      intersection_rect_ = PhysicalRect::EnclosingRect(
          root_geometry.root_to_document_transform
              .MapQuad(FloatQuad(FloatRect(intersection_rect_)))
              .BoundingBox());
    }
  } else {
    intersection_rect_ = PhysicalRect();
  }
  // Map root_rect_ from root's coordinate system to absolute coordinates.
  root_rect_ =
      PhysicalRect::EnclosingRect(root_geometry.root_to_document_transform
                                      .MapQuad(FloatQuad(FloatRect(root_rect_)))
                                      .BoundingBox());

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.

  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.

  if (does_intersect) {
    const PhysicalRect& comparison_rect =
        ShouldTrackFractionOfRoot() ? root_rect_ : target_rect_;
    if (comparison_rect.IsEmpty()) {
      intersection_ratio_ = 1;
    } else {
      const PhysicalSize& intersection_size = intersection_rect_.size;
      const float intersection_area = intersection_size.width.ToFloat() *
                                      intersection_size.height.ToFloat();
      const PhysicalSize& comparison_size = comparison_rect.size;
      const float area_of_interest =
          comparison_size.width.ToFloat() * comparison_size.height.ToFloat();
      intersection_ratio_ = std::min(intersection_area / area_of_interest, 1.f);
    }
    threshold_index_ =
        FirstThresholdGreaterThan(intersection_ratio_, thresholds);
  } else {
    intersection_ratio_ = 0;
    threshold_index_ = 0;
  }
  if (IsIntersecting() && ShouldComputeVisibility() &&
      ComputeIsVisible(target, target_rect_))
    flags_ |= kIsVisible;

  if (flags_ & kShouldConvertToCSSPixels) {
    FloatRect target_float_rect(target_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(target_float_rect, *target);
    target_rect_ = PhysicalRect::EnclosingRect(target_float_rect);
    FloatRect intersection_float_rect(intersection_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(intersection_float_rect, *target);
    intersection_rect_ = PhysicalRect::EnclosingRect(intersection_float_rect);
    FloatRect root_float_rect(root_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(root_float_rect, *root);
    root_rect_ = PhysicalRect::EnclosingRect(root_float_rect);
  }
}

bool IntersectionGeometry::ClipToRoot(const LayoutObject* root,
                                      const LayoutObject* target,
                                      const PhysicalRect& root_rect,
                                      PhysicalRect& intersection_rect) {
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  const LayoutBox* local_ancestor = nullptr;
  if (!RootIsImplicit() || root->GetDocument().IsInMainFrame())
    local_ancestor = ToLayoutBox(root);

  const LayoutView* layout_view = target->GetDocument().GetLayoutView();

  unsigned flags = kDefaultVisualRectFlags | kEdgeInclusive;
  if (!layout_view->NeedsPaintPropertyUpdate() &&
      !layout_view->DescendantNeedsPaintPropertyUpdate()) {
    flags |= kUseGeometryMapper;
  }
  bool does_intersect = target->MapToVisualRectInAncestorSpace(
      local_ancestor, intersection_rect, static_cast<VisualRectFlags>(flags));

  // Note that this early-return for (!local_ancestor) skips clipping to the
  // root_rect. That's ok because the only scenario where local_ancestor is
  // null is an implicit root and running inside an OOPIF, in which case there
  // can't be any root margin applied to root_rect (root margin is disallowed
  // for implicit-root cross-origin observation). So the default behavior of
  // MapToVisualRectInAncestorSpace will have already done the right thing WRT
  // clipping to the implicit root.
  if (!does_intersect || !local_ancestor)
    return does_intersect;

  if (local_ancestor->HasOverflowClip()) {
    intersection_rect.Move(
        -PhysicalOffset(LayoutPoint(local_ancestor->ScrollOrigin()) +
                        local_ancestor->ScrolledContentOffset()));
  }
  LayoutRect root_clip_rect = root_rect.ToLayoutRect();
  // TODO(szager): This flipping seems incorrect because root_rect is already
  // physical.
  local_ancestor->DeprecatedFlipForWritingMode(root_clip_rect);
  return does_intersect &
         intersection_rect.InclusiveIntersect(PhysicalRect(root_clip_rect));
}

unsigned IntersectionGeometry::FirstThresholdGreaterThan(
    float ratio,
    const Vector<float>& thresholds) const {
  unsigned result = 0;
  while (result < thresholds.size() && thresholds[result] <= ratio)
    ++result;
  return result;
}

}  // namespace blink
