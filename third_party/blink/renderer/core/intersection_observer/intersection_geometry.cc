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

// Return true if ancestor is in the containing block chain above descendant.
bool IsContainingBlockChainDescendant(const LayoutObject* descendant,
                                      const LayoutObject* ancestor) {
  if (!ancestor || !descendant || ancestor == descendant)
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
  if ((flags & IntersectionGeometry::kShouldUseReplacedContentRect) &&
      target->IsLayoutEmbeddedContent()) {
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

// Validates the given target element and returns its LayoutObject
LayoutObject* GetTargetLayoutObject(const Element& target_element) {
  if (!target_element.isConnected())
    return nullptr;
  LayoutObject* target = target_element.GetLayoutObject();
  if (!target || (!target->IsBoxModelObject() && !target->IsText()))
    return nullptr;
  // If the target is inside a locked subtree, it isn't ever visible.
  if (UNLIKELY(target->GetFrameView()->IsDisplayLocked() ||
               DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
                   target_element))) {
    return nullptr;
  }

  DCHECK(!target_element.GetDocument().View()->NeedsLayout());
  return target;
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
    IntersectionGeometry::kShouldUseReplacedContentRect |
    IntersectionGeometry::kShouldConvertToCSSPixels |
    IntersectionGeometry::kShouldUseCachedRects |
    IntersectionGeometry::kUseOverflowClipEdge;

}  // namespace

IntersectionGeometry::RootGeometry::RootGeometry(const LayoutObject* root,
                                                 const Vector<Length>& margin) {
  if (!root || !root->GetNode() || !root->GetNode()->isConnected() ||
      !root->IsBox())
    return;
  zoom = root->StyleRef().EffectiveZoom();
  local_root_rect = InitializeRootRect(root, margin);
  TransformState transform_state(TransformState::kApplyTransformDirection);
  root->MapLocalToAncestor(nullptr, transform_state, 0);
  root_to_document_transform = transform_state.AccumulatedTransform();
}

// If root_node is non-null, it is treated as the explicit root of an
// IntersectionObserver; if it is valid, its LayoutObject is returned.
//
// If root_node is null, returns the object to be used as the implicit root
// for a given target.
//
//   https://w3c.github.io/IntersectionObserver/#dom-intersectionobserver-root
const LayoutObject* IntersectionGeometry::GetRootLayoutObjectForTarget(
    const Node* root_node,
    LayoutObject* target,
    bool check_containing_block_chain) {
  if (!root_node)
    return target ? LocalRootView(*target) : nullptr;
  if (!root_node->isConnected())
    return nullptr;

  LayoutObject* root = nullptr;
  if (root_node->IsDocumentNode()) {
    root = To<Document>(root_node)->GetLayoutView();
  } else {
    root = root_node->GetLayoutObject();
  }
  if (target && check_containing_block_chain &&
      !IsContainingBlockChainDescendant(target, root)) {
    root = nullptr;
  }
  return root;
}

IntersectionGeometry::IntersectionGeometry(const Node* root_node,
                                           const Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           const Vector<Length>& target_margin,
                                           unsigned flags,
                                           CachedRects* cached_rects)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  // Only one of root_margin or target_margin can be specified.
  DCHECK(root_margin.empty() || target_margin.empty());

  if (cached_rects)
    cached_rects->valid = false;
  if (!root_node)
    flags_ |= kRootIsImplicit;
  LayoutObject* target = GetTargetLayoutObject(target_element);
  if (!target)
    return;
  const LayoutObject* root =
      GetRootLayoutObjectForTarget(root_node, target, !ShouldUseCachedRects());
  if (!root)
    return;
  RootGeometry root_geometry(root, root_margin);
  ComputeGeometry(root_geometry, root, target, thresholds, target_margin,
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
  if (cached_rects)
    cached_rects->valid = false;
  LayoutObject* target = GetTargetLayoutObject(target_element);
  if (!target)
    return;
  const LayoutObject* root = GetRootLayoutObjectForTarget(
      &explicit_root, target, !ShouldUseCachedRects());
  if (!root)
    return;
  ComputeGeometry(root_geometry, root, target, thresholds, target_margin,
                  cached_rects);
}

void IntersectionGeometry::ComputeGeometry(const RootGeometry& root_geometry,
                                           const LayoutObject* root,
                                           const LayoutObject* target,
                                           const Vector<float>& thresholds,
                                           const Vector<Length>& target_margin,
                                           CachedRects* cached_rects) {
  DCHECK(cached_rects || !ShouldUseCachedRects());
  flags_ |= kDidComputeGeometry;
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
  if (property_container) {
    gfx::RectF target_rect(target_rect_);
    target_rect.Offset(gfx::Vector2dF(target->FirstFragment().PaintOffset()));
    GeometryMapper::SourceToDestinationRect(container_properties.Transform(),
                                            target->GetDocument()
                                                .GetLayoutView()
                                                ->FirstFragment()
                                                .LocalBorderBoxProperties()
                                                .Transform(),
                                            target_rect);
    target_rect_ = PhysicalRect::EnclosingRect(target_rect);
  } else {
    target_rect_ = target->LocalToAncestorRect(target_rect_, nullptr);
  }
  if (does_intersect) {
    if (RootIsImplicit()) {
      // Generate matrix to transform from the space of the implicit root to
      // the absolute coordinates of the target document.
      TransformState implicit_root_to_target_document_transform(
          TransformState::kUnapplyInverseTransformDirection);
      target->GetDocument().GetLayoutView()->MapAncestorToLocal(
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
          root_geometry.root_to_document_transform
              .MapQuad(gfx::QuadF(gfx::RectF(intersection_rect_)))
              .BoundingBox());
      unclipped_intersection_rect_ = PhysicalRect::EnclosingRect(
          root_geometry.root_to_document_transform
              .MapQuad(gfx::QuadF(gfx::RectF(unclipped_intersection_rect_)))
              .BoundingBox());
    }
  } else {
    intersection_rect_ = PhysicalRect();
  }
  // Map root_rect_ from root's coordinate system to absolute coordinates.
  root_rect_ = PhysicalRect::EnclosingRect(
      root_geometry.root_to_document_transform
          .MapQuad(gfx::QuadF(gfx::RectF(root_rect_)))
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

  if (cached_rects)
    cached_rects->valid = true;
}

bool IntersectionGeometry::ClipToRoot(const LayoutObject* root,
                                      const LayoutObject* target,
                                      const PhysicalRect& root_rect,
                                      PhysicalRect& unclipped_intersection_rect,
                                      PhysicalRect& intersection_rect,
                                      CachedRects* cached_rects) {
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

}  // namespace blink
