// Copyright 2016 The Chromium Authors
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
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

namespace {

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
void ApplyMargin(
    PhysicalRect& expand_rect,
    const Vector<Length>& margin,
    float zoom,
    const absl::optional<PhysicalRect>& resolution_rect = absl::nullopt) {
  if (margin.empty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  const PhysicalRect& rect = resolution_rect.value_or(expand_rect);
  NGPhysicalBoxStrut outsets(ComputeMargin(margin[0], rect.Height(), zoom),
                             ComputeMargin(margin[1], rect.Width(), zoom),
                             ComputeMargin(margin[2], rect.Height(), zoom),
                             ComputeMargin(margin[3], rect.Width(), zoom));
  expand_rect.Expand(outsets);
}

// Returns the root intersect rect for the given root object, with the given
// margins applied, in the coordinate system of the root object.
//
//   https://w3c.github.io/IntersectionObserver/#intersectionobserver-root-intersection-rectangle
PhysicalRect InitializeRootRect(const LayoutObject* root,
                                const Vector<Length>& margin) {
  DCHECK(margin.empty() || margin.size() == 4);
  PhysicalRect result;
  auto* layout_view = DynamicTo<LayoutView>(root);
  if (layout_view && root->GetDocument().GetFrame()->IsOutermostMainFrame()) {
    // The main frame is a bit special as the scrolling viewport can differ in
    // size from the LayoutView itself. There's two situations this occurs in:
    // 1) The ForceZeroLayoutHeight quirk setting is used in Android WebView for
    // compatibility and sets the initial-containing-block's (a.k.a.
    // LayoutView) height to 0. Thus, we can't use its size for intersection
    // testing. Use the FrameView geometry instead.
    // 2) An element wider than the ICB can cause us to resize the FrameView so
    // we can zoom out to fit the entire element width.
    result = layout_view->OverflowClipRect(PhysicalOffset());
  } else if (root->IsBox() && root->IsScrollContainer()) {
    result = To<LayoutBox>(root)->PhysicalContentBoxRect();
  } else if (root->IsBox()) {
    result = To<LayoutBox>(root)->PhysicalBorderBoxRect();
  } else {
    result = To<LayoutInline>(root)->PhysicalLinesBoundingBox();
  }
  ApplyMargin(result, margin, root->StyleRef().EffectiveZoom());
  return result;
}

PhysicalRect GetBoxBounds(const LayoutBox* box, bool use_overflow_clip_edge) {
  PhysicalRect bounds(box->PhysicalBorderBoxRect());
  // Only use overflow clip rect if we need to use overflow clip edge and
  // overflow clip margin may have an effect, meaning we clip to the overflow
  // clip edge and not something else.
  if (use_overflow_clip_edge && box->ShouldApplyOverflowClipMargin()) {
    // OverflowClipRect() may be smaller than PhysicalBorderBoxRect().
    bounds.Unite(box->OverflowClipRect(PhysicalOffset()));
  }
  return bounds;
}

// Return the bounding box of target in target's own coordinate system, also
// return a bool indicating whether the target rect before margin application
// was empty.
std::pair<PhysicalRect, bool> InitializeTargetRect(const LayoutObject* target,
                                                   unsigned flags,
                                                   const Vector<Length>& margin,
                                                   const LayoutObject* root) {
  std::pair<PhysicalRect, bool> result;
  if (flags & IntersectionGeometry::kForFrameViewportIntersection) {
    result.first = To<LayoutEmbeddedContent>(target)->ReplacedContentRect();
  } else if (target->IsBox()) {
    result.first =
        GetBoxBounds(To<LayoutBox>(target),
                     flags & IntersectionGeometry::kUseOverflowClipEdge);
  } else if (target->IsLayoutInline()) {
    result.first = PhysicalRect::EnclosingRect(
        To<LayoutInline>(target)->LocalBoundingBoxRectF());
  } else {
    result.first = To<LayoutText>(target)->PhysicalLinesBoundingBox();
  }
  result.second = result.first.IsEmpty();
  ApplyMargin(result.first, margin, root->StyleRef().EffectiveZoom(),
              InitializeRootRect(root, {} /* margin */));
  return result;
}

// Returns true if target has visual effects applied, or if rect, given in
// absolute coordinates, is overlapped by any content painted after target
//
//   https://w3c.github.io/IntersectionObserver/v2/#calculate-visibility-algo
bool ComputeIsVisible(const LayoutObject* target, const PhysicalRect& rect) {
  if (!target->GetDocument().GetFrame() ||
      target->GetDocument().GetFrame()->LocalFrameRoot().GetOcclusionState() !=
          mojom::blink::FrameOcclusionState::kGuaranteedNotOccluded) {
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

bool CanUseGeometryMapper(const LayoutObject* object) {
  // This checks for cases where we didn't just complete a successful lifecycle
  // update, e.g., if the frame is throttled.
  LayoutView* layout_view = object->GetDocument().GetLayoutView();
  return layout_view && !layout_view->NeedsPaintPropertyUpdate() &&
         !layout_view->DescendantNeedsPaintPropertyUpdate();
}

static const unsigned kConstructorFlagsMask =
    IntersectionGeometry::kShouldReportRootBounds |
    IntersectionGeometry::kShouldComputeVisibility |
    IntersectionGeometry::kShouldTrackFractionOfRoot |
    IntersectionGeometry::kForFrameViewportIntersection |
    IntersectionGeometry::kShouldConvertToCSSPixels |
    IntersectionGeometry::kUseOverflowClipEdge;

}  // namespace

IntersectionGeometry::RootGeometry::RootGeometry(const LayoutObject* root,
                                                 const Vector<Length>& margin) {
  if (!root || !root->GetNode() || !root->GetNode()->isConnected() ||
      // TODO(crbug.com/1456208): Support inline root.
      !root->IsBox()) {
    return;
  }
  zoom = root->StyleRef().EffectiveZoom();
  local_root_rect = InitializeRootRect(root, margin);
  TransformState transform_state(TransformState::kApplyTransformDirection);
  root->MapLocalToAncestor(nullptr, transform_state, 0);
  root_to_document_transform = transform_state.AccumulatedTransform();
}

const LayoutObject* IntersectionGeometry::GetExplicitRootLayoutObject(
    const Node& root_node) {
  if (!root_node.isConnected()) {
    return nullptr;
  }
  if (root_node.IsDocumentNode()) {
    return To<Document>(root_node).GetLayoutView();
  }
  return root_node.GetLayoutObject();
}

IntersectionGeometry::IntersectionGeometry(const Node* root_node,
                                           const Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           const Vector<Length>& target_margin,
                                           unsigned flags,
                                           CachedRects* cached_rects)
    : flags_(flags & kConstructorFlagsMask) {
  // Only one of root_margin or target_margin can be specified.
  DCHECK(root_margin.empty() || target_margin.empty());

  if (!root_node)
    flags_ |= kRootIsImplicit;
  RootAndTarget root_and_target =
      PrepareComputeGeometry(root_node, target_element, cached_rects);
  if (root_and_target.relationship == RootAndTarget::kInvalid) {
    return;
  }
  RootGeometry root_geometry(root_and_target.root, root_margin);
  ComputeGeometry(root_geometry, root_and_target, thresholds, target_margin,
                  cached_rects);
}

IntersectionGeometry::IntersectionGeometry(const RootGeometry& root_geometry,
                                           const Node& explicit_root,
                                           const Element& target_element,
                                           const Vector<float>& thresholds,
                                           const Vector<Length>& target_margin,
                                           unsigned flags,
                                           CachedRects* cached_rects)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  auto root_and_target =
      PrepareComputeGeometry(&explicit_root, target_element, cached_rects);
  if (root_and_target.relationship == RootAndTarget::kInvalid) {
    return;
  }
  ComputeGeometry(root_geometry, root_and_target, thresholds, target_margin,
                  cached_rects);
}

IntersectionGeometry::RootAndTarget::RootAndTarget(
    const Node* root_node,
    const Element& target_element)
    : target(GetTargetLayoutObject(target_element)),
      root(target ? GetRootLayoutObject(root_node) : nullptr) {
  ComputeRelationship(!root_node);
}

// Validates the given target element and returns its LayoutObject
const LayoutObject* IntersectionGeometry::RootAndTarget::GetTargetLayoutObject(
    const Element& target_element) {
  if (!target_element.isConnected()) {
    return nullptr;
  }
  LayoutObject* target = target_element.GetLayoutObject();
  if (!target || (!target->IsBoxModelObject() && !target->IsText())) {
    return nullptr;
  }
  // If the target is inside a locked subtree, it isn't ever visible.
  if (UNLIKELY(target->GetFrameView()->IsDisplayLocked() ||
               DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
                   target_element))) {
    return nullptr;
  }

  DCHECK(!target_element.GetDocument().View()->NeedsLayout());
  return target;
}

// If root_node is non-null, it is treated as the explicit root of an
// IntersectionObserver; if it is valid, its LayoutObject is returned.
//
// If root_node is null, returns the object to be used to compute intersection
// for a given target with the implicit root. Note that if the target is in
// a remote frame, the returned object is the LayoutView of the local frame
// root instead of the topmost main frame.
//
//   https://w3c.github.io/IntersectionObserver/#dom-intersectionobserver-root
const LayoutObject* IntersectionGeometry::RootAndTarget::GetRootLayoutObject(
    const Node* root_node) const {
  if (root_node) {
    return GetExplicitRootLayoutObject(*root_node);
  }
  if (const LocalFrame* frame = target->GetDocument().GetFrame()) {
    return frame->LocalFrameRoot().ContentLayoutObject();
  }
  return nullptr;
}

void IntersectionGeometry::RootAndTarget::ComputeRelationship(
    bool root_is_implicit) {
  if (!root || !target || root == target) {
    relationship = kInvalid;
    return;
  }
  if (root_is_implicit && !target->GetFrame()->IsOutermostMainFrame()) {
    relationship = kTargetInSubFrame;
    return;
  }
  if (target->GetFrame() != root->GetFrame()) {
    // The case of different frame with implicit root has been covered by the
    // previous condition.
    DCHECK(!root_is_implicit);
    // The target and the explicit root are required to be in the same frame.
    relationship = kInvalid;
    return;
  }
  bool has_intermediate_clippers = false;
  const LayoutObject* container = target;
  while (container != root) {
    has_filter |= container->HasFilterInducingProperty();
    // Don't check for filters if we've already found one.
    LayoutObject::AncestorSkipInfo skip_info(root, !has_filter);
    container = container->Container(&skip_info);
    if (!has_filter) {
      has_filter = skip_info.FilterSkipped();
    }
    if (!container || skip_info.AncestorSkipped()) {
      // The root is not in the containing block chain of the target.
      relationship = kInvalid;
      return;
    }
    if (container != root && container->HasNonVisibleOverflow() &&
        // Non-scrollable scrollers are ignored.
        To<LayoutBox>(container)->HasLayoutOverflow()) {
      has_intermediate_clippers = true;
    }
  }
  if (has_intermediate_clippers) {
    relationship = kScrollableWithIntermediateClippers;
  } else if (root->IsScrollContainer() &&
             To<LayoutBox>(root)->HasLayoutOverflow()) {
    relationship = kScrollableByRootOnly;
  } else {
    relationship = kNotScrollable;
  }
}

IntersectionGeometry::RootAndTarget
IntersectionGeometry::PrepareComputeGeometry(const Node* root_node,
                                             const Element& target_element,
                                             CachedRects* cached_rects) {
  if (cached_rects) {
    if (cached_rects->valid) {
      flags_ |= kShouldUseCachedRects;
    }
    cached_rects->valid = false;
  }
  RootAndTarget root_and_target(root_node, target_element);

  if (ShouldUseCachedRects()) {
    CHECK(!RootIsImplicit() ||
          RuntimeEnabledFeatures::IntersectionOptimizationEnabled());
    // Cached rects can only be used if there are no scrollable objects in the
    // hierarchy between target and root (a scrollable root is ok). The reason
    // is that a scroll change in an intermediate scroller would change the
    // intersection geometry, but it would not properly trigger an invalidation
    // of the cached rects.
    auto legacy_can_use_cached_rects = [root_node, &target_element]() {
      if (LayoutObject* target = target_element.GetLayoutObject()) {
        PaintLayer* root_layer = target->GetDocument().GetLayoutView()->Layer();
        if (!root_layer) {
          return false;
        }
        if (LayoutBox* scroller = target->DeprecatedEnclosingScrollableBox()) {
          if (scroller->GetNode() == root_node) {
            return true;
          }
        }
      }
      return false;
    };
    if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()
            // TODO(wangxianzhu): Don't use cached rects for implicit root for
            // now because that would expose some under-invalidaiton bugs.
            //
            ? (RootIsImplicit() ||
               (root_and_target.relationship != RootAndTarget::kNotScrollable &&
                root_and_target.relationship !=
                    RootAndTarget::kScrollableByRootOnly))
            : !legacy_can_use_cached_rects()) {
      flags_ &= ~kShouldUseCachedRects;
    }
  }

  return root_and_target;
}

void IntersectionGeometry::ComputeGeometry(const RootGeometry& root_geometry,
                                           const RootAndTarget& root_and_target,
                                           const Vector<float>& thresholds,
                                           const Vector<Length>& target_margin,
                                           CachedRects* cached_rects) {
  CHECK_GE(thresholds.size(), 1u);
  DCHECK(cached_rects || !ShouldUseCachedRects());
  flags_ |= kDidComputeGeometry;

  const LayoutObject* root = root_and_target.root;
  const LayoutObject* target = root_and_target.target;
  CHECK(root);
  CHECK(target);

  // Initially:
  //   target_rect_ is in target's coordinate system
  //   root_rect_ is in root's coordinate system
  //   The coordinate system for unclipped_intersection_rect_ depends on whether
  //       or not we can use previously cached geometry...
  bool pre_margin_target_rect_is_empty;
  if (ShouldUseCachedRects()) {
    target_rect_ = cached_rects->local_target_rect;
    pre_margin_target_rect_is_empty =
        cached_rects->pre_margin_target_rect_is_empty;

    // The cached intersection rect has already been mapped/clipped up to the
    // root, except that the root's scroll offset and overflow clip have not
    // been applied.
    unclipped_intersection_rect_ =
        cached_rects->unscrolled_unclipped_intersection_rect;
  } else {
    std::tie(target_rect_, pre_margin_target_rect_is_empty) =
        InitializeTargetRect(target, flags_, target_margin, root);
    // We have to map/clip target_rect_ up to the root, so we begin with the
    // intersection rect in target's coordinate system. After ClipToRoot, it
    // will be in root's coordinate system.
    unclipped_intersection_rect_ = target_rect_;
  }
  if (cached_rects) {
    cached_rects->local_target_rect = target_rect_;
    cached_rects->pre_margin_target_rect_is_empty =
        pre_margin_target_rect_is_empty;
  }
  root_rect_ = root_geometry.local_root_rect;

  bool does_intersect =
      ClipToRoot(root, target, root_rect_, unclipped_intersection_rect_,
                 intersection_rect_, cached_rects);

  // Map target_rect_ to absolute coordinates for target's document.
  // GeometryMapper is faster, so we use it when possible; otherwise, fall back
  // to LocalToAncestorRect.
  PropertyTreeStateOrAlias container_properties =
      PropertyTreeState::Uninitialized();
  const LayoutObject* property_container =
      CanUseGeometryMapper(target)
          ? target->GetPropertyContainer(nullptr, &container_properties)
          : nullptr;
  gfx::Transform target_to_document_transform;
  if (property_container) {
    target_to_document_transform =
        GeometryMapper::SourceToDestinationProjection(
            container_properties.Transform(), target->View()
                                                  ->FirstFragment()
                                                  .LocalBorderBoxProperties()
                                                  .Transform());
    target_rect_.Move(target->FirstFragment().PaintOffset());
  } else {
    TransformState transform_state(TransformState::kApplyTransformDirection);
    target->MapLocalToAncestor(nullptr, transform_state, 0);
    target_to_document_transform = transform_state.AccumulatedTransform();
  }
  target_rect_ = PhysicalRect::EnclosingRect(
      target_to_document_transform.MapRect(gfx::RectF(target_rect_)));

  if (does_intersect) {
    if (RootIsImplicit()) {
      // Generate matrix to transform from the space of the implicit root to
      // the absolute coordinates of the target document.
      TransformState implicit_root_to_target_document_transform(
          TransformState::kUnapplyInverseTransformDirection);
      target->View()->MapAncestorToLocal(
          nullptr, implicit_root_to_target_document_transform,
          kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);
      gfx::Transform matrix =
          implicit_root_to_target_document_transform.AccumulatedTransform()
              .InverseOrIdentity();
      intersection_rect_ = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(gfx::QuadF(gfx::RectF(intersection_rect_)))
              .BoundingBox());
      unclipped_intersection_rect_ = PhysicalRect::EnclosingRect(
          matrix
              .ProjectQuad(gfx::QuadF(gfx::RectF(unclipped_intersection_rect_)))
              .BoundingBox());
      // intersection_rect_ is in the coordinate system of the implicit root;
      // map it down the to absolute coordinates for the target's document.
    } else {
      // intersection_rect_ is in root's coordinate system; map it up to
      // absolute coordinates for target's containing document (which is the
      // same as root's document).
      intersection_rect_ = PhysicalRect::EnclosingRect(
          root_geometry.root_to_document_transform.MapRect(
              gfx::RectF(intersection_rect_)));
      unclipped_intersection_rect_ = PhysicalRect::EnclosingRect(
          root_geometry.root_to_document_transform.MapRect(
              gfx::RectF(unclipped_intersection_rect_)));
    }
  } else {
    intersection_rect_ = PhysicalRect();
  }
  // Map root_rect_ from root's coordinate system to absolute coordinates.
  root_rect_ = PhysicalRect::EnclosingRect(
      root_geometry.root_to_document_transform.MapRect(gfx::RectF(root_rect_)));

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
    // Note that if we are checking whether target is empty, we have to consider
    // the fact that we might have padded the rect with a target margin. If we
    // did, `pre_margin_target_rect_is_empty` would be true. Use this
    // information to force the rect to be empty for the purposes of this
    // computation. Note that it could also be the case that the rect started as
    // non-empty and was transformed to be empty. In this case, we rely on
    // target_rect_.IsEmpty() to be true, so we need to check the rect itself as
    // well.
    // In the fraction of root case, we can just check the comparison rect.
    bool empty_override =
        !ShouldTrackFractionOfRoot() && pre_margin_target_rect_is_empty;
    if (comparison_rect.IsEmpty() || empty_override) {
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
      ComputeIsVisible(target, target_rect_)) {
    flags_ |= kIsVisible;
  }

  if (flags_ & kShouldConvertToCSSPixels) {
    gfx::RectF target_float_rect(target_rect_);
    AdjustForAbsoluteZoom::AdjustRectF(target_float_rect, *target);
    target_rect_ = PhysicalRect::EnclosingRect(target_float_rect);
    gfx::RectF intersection_float_rect(intersection_rect_);
    AdjustForAbsoluteZoom::AdjustRectF(intersection_float_rect, *target);
    intersection_rect_ = PhysicalRect::EnclosingRect(intersection_float_rect);
    gfx::RectF root_float_rect(root_rect_);
    AdjustForAbsoluteZoom::AdjustRectF(root_float_rect, *root);
    root_rect_ = PhysicalRect::EnclosingRect(root_float_rect);
  }

  min_scroll_delta_to_update_ = ComputeMinScrollDeltaToUpdate(
      root_and_target, target_to_document_transform,
      root_geometry.root_to_document_transform, thresholds);

  if (cached_rects) {
    cached_rects->min_scroll_delta_to_update = min_scroll_delta_to_update_;
    cached_rects->valid = true;
  }
}

bool IntersectionGeometry::ClipToRoot(const LayoutObject* root,
                                      const LayoutObject* target,
                                      const PhysicalRect& root_rect,
                                      PhysicalRect& unclipped_intersection_rect,
                                      PhysicalRect& intersection_rect,
                                      CachedRects* cached_rects) {
  if (!root->IsBox()) {
    return false;
  }
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  const LayoutBox* local_ancestor = nullptr;
  if (!RootIsImplicit() ||
      root->GetDocument().GetFrame()->IsOutermostMainFrame())
    local_ancestor = To<LayoutBox>(root);

  unsigned flags = kDefaultVisualRectFlags | kEdgeInclusive |
                   kDontApplyMainFrameOverflowClip;
  if (CanUseGeometryMapper(target))
    flags |= kUseGeometryMapper;

  bool does_intersect;
  if (ShouldUseCachedRects()) {
    does_intersect = cached_rects->does_intersect;
  } else {
    does_intersect = target->MapToVisualRectInAncestorSpace(
        local_ancestor, unclipped_intersection_rect,
        static_cast<VisualRectFlags>(flags));
  }
  if (cached_rects) {
    cached_rects->unscrolled_unclipped_intersection_rect =
        unclipped_intersection_rect;
    cached_rects->does_intersect = does_intersect;
  }

  intersection_rect = PhysicalRect();

  // If the target intersects with the unclipped root, calculate the clipped
  // intersection.
  if (does_intersect) {
    intersection_rect = unclipped_intersection_rect;
    if (local_ancestor) {
      if (local_ancestor->IsScrollContainer()) {
        PhysicalOffset scroll_offset =
            -(PhysicalOffset(local_ancestor->ScrollOrigin()) +
              PhysicalOffset(
                  local_ancestor->PixelSnappedScrolledContentOffset()));
        intersection_rect.Move(scroll_offset);
        unclipped_intersection_rect.Move(scroll_offset);
      }
      LayoutRect root_clip_rect = root_rect.ToLayoutRect();
      // TODO(szager): This flipping seems incorrect because root_rect is
      // already physical.
      local_ancestor->DeprecatedFlipForWritingMode(root_clip_rect);
      does_intersect &=
          intersection_rect.InclusiveIntersect(PhysicalRect(root_clip_rect));
    } else {
      // Note that we don't clip to root_rect here. That's ok because
      // (!local_ancestor) implies that the root is implicit and the
      // main frame is remote, in which case there can't be any root margin
      // applied to root_rect (root margin is disallowed for implicit-root
      // cross-origin observation). We still need to apply the remote main
      // frame's overflow clip here, because the
      // kDontApplyMainFrameOverflowClip flag above, means it hasn't been
      // done yet.
      LocalFrame* local_root_frame = root->GetDocument().GetFrame();
      gfx::Rect clip_rect(local_root_frame->RemoteViewportIntersection());
      if (clip_rect.IsEmpty()) {
        intersection_rect = PhysicalRect();
        does_intersect = false;
      } else {
        // Map clip_rect from the coordinate system of the local root frame to
        // the coordinate system of the remote main frame.
        clip_rect = ToPixelSnappedRect(
            local_root_frame->ContentLayoutObject()->LocalToAncestorRect(
                PhysicalRect(clip_rect), nullptr,
                kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform));
        does_intersect &=
            intersection_rect.InclusiveIntersect(PhysicalRect(clip_rect));
      }
    }
  }

  return does_intersect;
}

unsigned IntersectionGeometry::FirstThresholdGreaterThan(
    float ratio,
    const Vector<float>& thresholds) const {
  unsigned result = 0;
  while (result < thresholds.size() && thresholds[result] <= ratio)
    ++result;
  return result;
}

gfx::Vector2dF IntersectionGeometry::ComputeMinScrollDeltaToUpdate(
    const RootAndTarget& root_and_target,
    const gfx::Transform& target_to_document_transform,
    const gfx::Transform& root_to_document_transform,
    const Vector<float>& thresholds) const {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return gfx::Vector2dF();
  }
  if (ShouldComputeVisibility()) {
    // We don't have enough data (e.g. the occluded area of target and the
    // occluding areas of the covering elements) to calculate the minimum
    // scroll delta affecting visibility.
    return gfx::Vector2dF();
  }
  if (root_and_target.relationship == RootAndTarget::kTargetInSubFrame) {
    return gfx::Vector2dF();
  }
  if (root_and_target.relationship == RootAndTarget::kNotScrollable) {
    // Intersection is not affected by scroll.
    return kInfiniteScrollDelta;
  }
  if (root_and_target.has_filter) {
    // With filters, the intersection rect can be non-empty even if root_rect_
    // and target_rect_ don't intersect.
    return gfx::Vector2dF();
  }
  if (!target_to_document_transform.IsIdentityOr2dTranslation() ||
      !root_to_document_transform.IsIdentityOr2dTranslation()) {
    return gfx::Vector2dF();
  }
  CHECK_GE(thresholds.size(), 1u);
  if (thresholds[0] == 1) {
    if (ShouldTrackFractionOfRoot()) {
      if (root_rect_.Width() > target_rect_.Width() ||
          root_rect_.Height() > target_rect_.Height()) {
        // The intersection rect (which is contained by target_rect_) can never
        // cover root_rect_ 100%.
        return kInfiniteScrollDelta;
      }
      if (target_rect_.Contains(root_rect_) &&
          root_and_target.relationship ==
              RootAndTarget::kScrollableWithIntermediateClippers) {
        // When target_rect_ fully contains root_rect_, whether the intersection
        // rect fully covers root_rect_ depends on intermediate clips, so there
        // is no minimum scroll delta.
        return gfx::Vector2dF();
      }
    } else {
      if (target_rect_.Width() > root_rect_.Width() ||
          target_rect_.Height() > root_rect_.Height()) {
        // The intersection rect (which is contained by root_rect_) can never
        // cover target_rect_ 100%.
        return kInfiniteScrollDelta;
      }
      if (root_rect_.Contains(target_rect_) &&
          root_and_target.relationship ==
              RootAndTarget::kScrollableWithIntermediateClippers) {
        // When root_rect_ fully contains target_rect_, whether target_rect_
        // is fully visible depends on intermediate clips, so there is no
        // minimum scroll delta.
        return gfx::Vector2dF();
      }
    }
    // Otherwise, we can skip update until target_rect_/root_rect_ is or isn't
    // fully contained by root_rect_/target_rect_.
    return gfx::Vector2dF(
        std::min((root_rect_.X() - target_rect_.X()).Abs(),
                 (root_rect_.Right() - target_rect_.Right()).Abs())
            .ToFloat(),
        std::min((root_rect_.Y() - target_rect_.Y()).Abs(),
                 (root_rect_.Bottom() - target_rect_.Bottom()).Abs())
            .ToFloat());
  }
  // Otherwise, if root_rect_ and target_rect_ intersect, the intersection
  // status may change on any scroll in case of intermediate clips or non-zero
  // thresholds. kMinimumThreshold equivalent to 0 for minimum scroll delta.
  if (root_rect_.IntersectsInclusively(target_rect_) &&
      (thresholds.size() != 1 || thresholds[0] > kMinimumThreshold ||
       root_and_target.relationship ==
           RootAndTarget::kScrollableWithIntermediateClippers ||
       IsForFrameViewportIntersection())) {
    return gfx::Vector2dF();
  }
  // Otherwise we can skip update until root_rect_ and target_rect_ is about
  // to change intersection status in either direction.
  return gfx::Vector2dF(std::min((root_rect_.Right() - target_rect_.X()).Abs(),
                                 (target_rect_.Right() - root_rect_.X()).Abs())
                            .ToFloat(),
                        std::min((root_rect_.Bottom() - target_rect_.Y()).Abs(),
                                 (target_rect_.Bottom() - root_rect_.Y()).Abs())
                            .ToFloat());
}

}  // namespace blink
