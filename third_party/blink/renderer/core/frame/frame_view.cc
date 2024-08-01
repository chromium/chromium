// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_view.h"

#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

FrameView::FrameView(const gfx::Rect& frame_rect)
    : EmbeddedContentView(frame_rect) {}

Frame& FrameView::GetFrame() const {
  if (const LocalFrameView* lfv = DynamicTo<LocalFrameView>(this))
    return lfv->GetFrame();
  return DynamicTo<RemoteFrameView>(this)->GetFrame();
}

bool FrameView::CanThrottleRenderingForPropagation() const {
  if (CanThrottleRendering())
    return true;
  Frame& frame = GetFrame();
  if (!frame.IsCrossOriginToNearestMainFrame())
    return false;
  if (frame.IsLocalFrame() && To<LocalFrame>(frame).IsHidden())
    return true;
  LocalFrame* parent_frame = DynamicTo<LocalFrame>(GetFrame().Tree().Parent());
  return (parent_frame && !frame.OwnerLayoutObject());
}

bool FrameView::DisplayLockedInParentFrame() {
  Frame& frame = GetFrame();
  LayoutEmbeddedContent* owner = frame.OwnerLayoutObject();
  if (!owner)
    return false;
  DCHECK(owner->GetFrameView());
  if (owner->GetFrameView()->IsDisplayLocked())
    return true;
  // We check the inclusive ancestor to determine whether the subtree is locked,
  // since the contents of the frame are in the subtree of the frame, so they
  // would be locked if the frame owner is itself locked.
  // We use a paint check here, since as lock as we don't allow paint, we are
  // display locked.
  return DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(*owner);
}

void FrameView::UpdateViewportIntersection(unsigned flags,
                                           bool needs_occlusion_tracking) {
  if (!(flags & IntersectionObservation::kImplicitRootObserversNeedUpdate)) {
    return;
  }

  // This should only run in child frames.
  Frame& frame = GetFrame();
  HTMLFrameOwnerElement* owner_element = frame.DeprecatedLocalOwner();
  if (!owner_element) {
    return;
  }

  Document& owner_document = owner_element->GetDocument();
  gfx::Rect viewport_intersection, mainframe_intersection;
  gfx::Transform main_frame_transform_matrix;
  DocumentLifecycle::LifecycleState parent_lifecycle_state =
      owner_document.Lifecycle().GetState();
  mojom::blink::FrameOcclusionState occlusion_state =
      owner_document.GetFrame()->GetOcclusionState();
  bool should_compute_occlusion =
      needs_occlusion_tracking &&
      occlusion_state ==
          mojom::blink::FrameOcclusionState::kGuaranteedNotOccluded &&
      parent_lifecycle_state >= DocumentLifecycle::kPrePaintClean;
  if (!should_compute_occlusion) {
    occlusion_state = mojom::blink::FrameOcclusionState::kUnknown;
  }

  LayoutEmbeddedContent* owner_layout_object =
      owner_element->GetLayoutEmbeddedContent();
  bool display_locked_in_parent_frame = DisplayLockedInParentFrame();
  if (!owner_layout_object || owner_layout_object->ContentSize().IsEmpty() ||
      (flags & IntersectionObservation::kAncestorFrameIsDetachedFromLayout) ||
      display_locked_in_parent_frame) {
    // The frame, or an ancestor frame, is detached from layout, not visible, or
    // zero size, or it's display locked in parent frame; leave
    // viewport_intersection empty, and signal the frame as occluded if
    // necessary.
    occlusion_state = mojom::blink::FrameOcclusionState::kPossiblyOccluded;
  } else if (parent_lifecycle_state >= DocumentLifecycle::kLayoutClean &&
             !owner_document.View()->NeedsLayout()) {
    unsigned geometry_flags =
        IntersectionGeometry::kForFrameViewportIntersection;
    if (should_compute_occlusion)
      geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;

    std::optional<IntersectionGeometry::RootGeometry> root_geometry;
    IntersectionGeometry geometry(
        /* root */ nullptr,
        /* target */ *owner_element,
        /* root_margin */ {},
        /* thresholds */ {IntersectionObserver::kMinimumThreshold},
        /* target_margin */ {},
        /* scroll_margin */ {}, geometry_flags, root_geometry);

    PhysicalRect new_rect_in_parent =
        PhysicalRect::FastAndLossyFromRectF(geometry.IntersectionRect());

    // Convert to DIP
    const auto& screen_info =
        frame.GetChromeClient().GetScreenInfo(*owner_document.GetFrame());
    new_rect_in_parent.Scale(1. / screen_info.device_scale_factor);

    // Movement as a proportion of frame size
    double horizontal_movement =
        new_rect_in_parent.Width()
            ? (new_rect_in_parent.X() - rect_in_parent_.X()).Abs() /
                  new_rect_in_parent.Width()
            : 0.0;
    double vertical_movement =
        new_rect_in_parent.Height()
            ? (new_rect_in_parent.Y() - rect_in_parent_.Y()).Abs() /
                  new_rect_in_parent.Height()
            : 0.0;
    if (new_rect_in_parent.size != rect_in_parent_.size ||
        horizontal_movement >
            FrameVisualProperties::MaxChildFrameScreenRectMovement() ||
        vertical_movement >
            FrameVisualProperties::MaxChildFrameScreenRectMovement()) {
      rect_in_parent_ = new_rect_in_parent;
      if (Page* page = GetFrame().GetPage()) {
        rect_in_parent_stable_since_ = page->Animator().Clock().CurrentTime();
      } else {
        rect_in_parent_stable_since_ = base::TimeTicks::Now();
      }
    }
    if (new_rect_in_parent.size != rect_in_parent_for_iov2_.size ||
        ((new_rect_in_parent.X() - rect_in_parent_for_iov2_.X()).Abs() +
             (new_rect_in_parent.Y() - rect_in_parent_for_iov2_.Y()).Abs() >
         LayoutUnit(FrameVisualProperties::
                        MaxChildFrameScreenRectMovementForIOv2()))) {
      rect_in_parent_for_iov2_ = new_rect_in_parent;
      if (Page* page = GetFrame().GetPage()) {
        rect_in_parent_stable_since_for_iov2_ =
            page->Animator().Clock().CurrentTime();
      } else {
        rect_in_parent_stable_since_for_iov2_ = base::TimeTicks::Now();
      }
    }
    if (should_compute_occlusion && !geometry.IsVisible())
      occlusion_state = mojom::blink::FrameOcclusionState::kPossiblyOccluded;

    // Generate matrix to transform from the space of the containing document
    // to the space of the iframe's contents.
    TransformState parent_frame_to_iframe_content_transform(
        TransformState::kUnapplyInverseTransformDirection);
    // First transform to box coordinates of the iframe element...
    owner_layout_object->MapAncestorToLocal(
        nullptr, parent_frame_to_iframe_content_transform, 0);
    // ... then apply content_box_offset to translate to the coordinate of the
    // child frame.
    parent_frame_to_iframe_content_transform.Move(
        owner_layout_object->PhysicalContentBoxOffset());
    gfx::Transform matrix =
        parent_frame_to_iframe_content_transform.AccumulatedTransform()
            .InverseOrIdentity();
    if (geometry.IsIntersecting()) {
      PhysicalRect intersection_rect = PhysicalRect::EnclosingRect(
          matrix
              .ProjectQuad(gfx::QuadF(gfx::RectF(geometry.IntersectionRect())))
              .BoundingBox());

      // Don't let EnclosingRect turn an empty rect into a non-empty one.
      if (intersection_rect.IsEmpty()) {
        viewport_intersection =
            gfx::Rect(ToFlooredPoint(intersection_rect.offset), gfx::Size());
      } else {
        viewport_intersection = ToEnclosingRect(intersection_rect);
      }

      // Because the geometry code uses enclosing rects, we may end up with an
      // intersection rect that is bigger than the rect we started with. Clamp
      // the size of the viewport intersection to the bounds of the iframe's
      // content rect.
      // TODO(crbug.com/1266676): This should be
      //   viewport_intersection.Intersect(gfx::Rect(gfx::Point(),
      //       owner_layout_object->ContentSize()));
      // but it exposes a bug of incorrect origin of viewport_intersection in
      // multicol.
      gfx::Point origin = viewport_intersection.origin();
      origin.SetToMax(gfx::Point());
      viewport_intersection.set_origin(origin);
      gfx::Size size = viewport_intersection.size();
      size.SetToMin(ToRoundedSize(owner_layout_object->ContentSize()));
      viewport_intersection.set_size(size);
    }

    PhysicalRect mainframe_intersection_rect;
    if (!geometry.UnclippedIntersectionRect().IsEmpty()) {
      mainframe_intersection_rect = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(gfx::QuadF(geometry.UnclippedIntersectionRect()))
              .BoundingBox());

      if (mainframe_intersection_rect.IsEmpty()) {
        mainframe_intersection = gfx::Rect(
            ToFlooredPoint(mainframe_intersection_rect.offset), gfx::Size());
      } else {
        mainframe_intersection = ToEnclosingRect(mainframe_intersection_rect);
      }
      // TODO(crbug.com/1266676): This should be
      //   mainframe_intersection.Intersect(gfx::Rect(gfx::Point(),
      //       owner_layout_object->ContentSize()));
      // but it exposes a bug of incorrect origin of mainframe_intersection in
      // multicol.
      gfx::Point origin = mainframe_intersection.origin();
      origin.SetToMax(gfx::Point());
      mainframe_intersection.set_origin(origin);
      gfx::Size size = mainframe_intersection.size();
      size.SetToMin(ToRoundedSize(owner_layout_object->ContentSize()));
      mainframe_intersection.set_size(size);
    }

    TransformState child_frame_to_root_frame(
        TransformState::kUnapplyInverseTransformDirection);
    // TODO: Should this be IsOutermostMainFrame()?
    if (owner_document.GetFrame()->LocalFrameRoot().IsMainFrame()) {
      child_frame_to_root_frame.Move(PhysicalOffset::FromPointFRound(
          gfx::PointF(frame.GetOutermostMainFrameScrollPosition())));
    }
    if (owner_layout_object) {
      owner_layout_object->MapAncestorToLocal(
          nullptr, child_frame_to_root_frame,
          kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);
      child_frame_to_root_frame.Move(
          owner_layout_object->PhysicalContentBoxOffset());
    }
    main_frame_transform_matrix =
        child_frame_to_root_frame.AccumulatedTransform();
  } else if (occlusion_state ==
             mojom::blink::FrameOcclusionState::kGuaranteedNotOccluded) {
    // If the parent LocalFrameView is throttled and out-of-date, then we can't
    // get any useful information.
    occlusion_state = mojom::blink::FrameOcclusionState::kUnknown;
  }

  // An iframe's content is always pixel-snapped, even if the iframe element has
  // non-pixel-aligned location.
  gfx::Transform pixel_snapped_transform = main_frame_transform_matrix;
  pixel_snapped_transform.Round2dTranslationComponents();

  SetViewportIntersection(mojom::blink::ViewportIntersectionState(
      viewport_intersection, mainframe_intersection, gfx::Rect(),
      occlusion_state, frame.GetOutermostMainFrameSize(),
      frame.GetOutermostMainFrameScrollPosition(), pixel_snapped_transform));

  UpdateFrameVisibility(!viewport_intersection.IsEmpty());

  if (ShouldReportMainFrameIntersection()) {
    gfx::Rect projected_rect = gfx::ToEnclosingRect(
        main_frame_transform_matrix
            .ProjectQuad(gfx::QuadF(gfx::RectF(mainframe_intersection)))
            .BoundingBox());
    // Return <0, 0, 0, 0> if there is no area.
    if (projected_rect.IsEmpty())
      projected_rect.set_origin(gfx::Point(0, 0));
    GetFrame().Client()->OnMainFrameIntersectionChanged(projected_rect);
  }

  // We don't throttle display:none iframes unless they are cross-origin and
  // ThrottleCrossOriginIframes is enabled, because in practice they are
  // sometimes used to drive UI logic. Zero-area iframes are only throttled if
  // they are also display:none.
  bool zero_viewport_intersection = viewport_intersection.IsEmpty();
  bool is_display_none = !owner_layout_object;
  bool has_zero_area = FrameRect().IsEmpty();
  bool should_throttle =
      (is_display_none || (zero_viewport_intersection && !has_zero_area));

  bool subtree_throttled = false;
  Frame* parent_frame = GetFrame().Tree().Parent();
  if (parent_frame && parent_frame->View()) {
    subtree_throttled =
        parent_frame->View()->CanThrottleRenderingForPropagation();
  }
  UpdateRenderThrottlingStatus(should_throttle, subtree_throttled,
                               display_locked_in_parent_frame);
}

void FrameView::UpdateFrameVisibility(bool intersects_viewport) {
  mojom::blink::FrameVisibility frame_visibility;
  if (LifecycleUpdatesThrottled())
    return;
  if (IsVisible()) {
    frame_visibility =
        intersects_viewport
            ? mojom::blink::FrameVisibility::kRenderedInViewport
            : mojom::blink::FrameVisibility::kRenderedOutOfViewport;
  } else {
    frame_visibility = mojom::blink::FrameVisibility::kNotRendered;
  }
  if (frame_visibility != frame_visibility_) {
    frame_visibility_ = frame_visibility;
    VisibilityChanged(frame_visibility);
  }
}

void FrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                             bool subtree_throttled,
                                             bool display_locked,
                                             bool recurse) {
  bool visibility_changed =
      (hidden_for_throttling_ || subtree_throttled_ || display_locked_) !=
      (hidden_for_throttling || subtree_throttled || display_locked);
  hidden_for_throttling_ = hidden_for_throttling;
  subtree_throttled_ = subtree_throttled;
  display_locked_ = display_locked;
  if (visibility_changed)
    VisibilityForThrottlingChanged();
  if (recurse) {
    for (Frame* child = GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (FrameView* child_view = child->View()) {
        child_view->UpdateRenderThrottlingStatus(
            child_view->IsHiddenForThrottling(),
            child_view->IsAttached() && CanThrottleRenderingForPropagation(),
            child_view->IsDisplayLocked(), true);
      }
    }
  }
}

bool FrameView::RectInParentIsStable(
    const base::TimeTicks& event_timestamp) const {
  if (event_timestamp - rect_in_parent_stable_since_ <
      base::Milliseconds(
          blink::FrameVisualProperties::MinScreenRectStableTimeMs())) {
    return false;
  }
  LocalFrameView* parent = ParentFrameView();
  if (!parent)
    return true;
  return parent->RectInParentIsStable(event_timestamp);
}

bool FrameView::RectInParentIsStableForIOv2(
    const base::TimeTicks& event_timestamp) const {
  if (event_timestamp - rect_in_parent_stable_since_for_iov2_ <
      base::Milliseconds(
          blink::FrameVisualProperties::MinScreenRectStableTimeMsForIOv2())) {
    return false;
  }
  LocalFrameView* parent = ParentFrameView();
  if (!parent) {
    return true;
  }
  return parent->RectInParentIsStableForIOv2(event_timestamp);
}
}  // namespace blink
