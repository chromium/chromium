// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_view.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "ui/gfx/transform.h"

namespace blink {

FrameView::FrameView(const IntRect& frame_rect)
    : EmbeddedContentView(frame_rect),
      frame_visibility_(blink::mojom::FrameVisibility::kRenderedInViewport) {}

Frame& FrameView::GetFrame() const {
  if (const LocalFrameView* lfv = DynamicTo<LocalFrameView>(this))
    return lfv->GetFrame();
  return DynamicTo<RemoteFrameView>(this)->GetFrame();
}

bool FrameView::CanThrottleRenderingForPropagation() const {
  if (CanThrottleRendering())
    return true;
  Frame& frame = GetFrame();
  if (!frame.IsCrossOriginToMainFrame())
    return false;
  if (frame.IsLocalFrame() && To<LocalFrame>(frame).IsHidden())
    return true;
  LocalFrame* parent_frame = DynamicTo<LocalFrame>(GetFrame().Tree().Parent());
  return (parent_frame && !frame.OwnerLayoutObject());
}

bool FrameView::DisplayLockedInParentFrame() {
  Frame& frame = GetFrame();
  LayoutEmbeddedContent* owner = frame.OwnerLayoutObject();
  // We check the inclusive ancestor to determine whether the subtree is locked,
  // since the contents of the frame are in the subtree of the frame, so they
  // would be locked if the frame owner is itself locked.
  return owner && DisplayLockUtilities::NearestLockedInclusiveAncestor(*owner);
}

void FrameView::UpdateViewportIntersection(unsigned flags,
                                           bool needs_occlusion_tracking) {
  if (!(flags & IntersectionObservation::kImplicitRootObserversNeedUpdate))
    return;

  // This should only run in child frames.
  Frame& frame = GetFrame();
  HTMLFrameOwnerElement* owner_element = frame.DeprecatedLocalOwner();
  if (!owner_element)
    return;

  Document& owner_document = owner_element->GetDocument();
  IntRect viewport_intersection, mainframe_intersection;
  TransformationMatrix main_frame_transform_matrix;
  DocumentLifecycle::LifecycleState parent_lifecycle_state =
      owner_document.Lifecycle().GetState();
  FrameOcclusionState occlusion_state =
      owner_document.GetFrame()->GetOcclusionState();
  bool should_compute_occlusion =
      needs_occlusion_tracking &&
      occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded &&
      parent_lifecycle_state >= DocumentLifecycle::kPrePaintClean;

  LayoutEmbeddedContent* owner_layout_object =
      owner_element->GetLayoutEmbeddedContent();
  if (!owner_layout_object || owner_layout_object->ContentSize().IsEmpty()) {
    // The frame is detached from layout, not visible, or zero size; leave
    // viewport_intersection empty, and signal the frame as occluded if
    // necessary.
    occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (parent_lifecycle_state >= DocumentLifecycle::kLayoutClean &&
             !owner_document.View()->NeedsLayout()) {
    unsigned geometry_flags =
        IntersectionGeometry::kShouldUseReplacedContentRect;
    if (should_compute_occlusion)
      geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;

    IntersectionGeometry geometry(nullptr, *owner_element, {} /* root_margin */,
                                  {IntersectionObserver::kMinimumThreshold},
                                  {} /* target_margin */, geometry_flags);
    PhysicalRect new_rect_in_parent = geometry.IntersectionRect();
    if (new_rect_in_parent.size != rect_in_parent_.size ||
        ((new_rect_in_parent.X() - rect_in_parent_.X()).Abs() +
             (new_rect_in_parent.Y() - rect_in_parent_.Y()).Abs() >
         LayoutUnit(kMaxChildFrameScreenRectMovement))) {
      rect_in_parent_ = new_rect_in_parent;
      if (Page* page = GetFrame().GetPage()) {
        rect_in_parent_stable_since_ = page->Animator().Clock().CurrentTime();
      } else {
        rect_in_parent_stable_since_ = base::TimeTicks::Now();
      }
    }
    if (should_compute_occlusion && !geometry.IsVisible())
      occlusion_state = FrameOcclusionState::kPossiblyOccluded;

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
    TransformationMatrix matrix =
        parent_frame_to_iframe_content_transform.AccumulatedTransform()
            .Inverse();
    if (geometry.IsIntersecting()) {
      PhysicalRect intersection_rect = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(FloatRect(geometry.IntersectionRect()))
              .BoundingBox());

      // Don't let EnclosingRect turn an empty rect into a non-empty one.
      if (intersection_rect.IsEmpty()) {
        viewport_intersection =
            IntRect(FlooredIntPoint(intersection_rect.offset), IntSize());
      } else {
        viewport_intersection = EnclosingIntRect(intersection_rect);
      }
    }

    PhysicalRect mainframe_intersection_rect;
    if (!geometry.UnclippedIntersectionRect().IsEmpty()) {
      mainframe_intersection_rect = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(FloatRect(geometry.UnclippedIntersectionRect()))
              .BoundingBox());

      if (mainframe_intersection_rect.IsEmpty()) {
        mainframe_intersection = IntRect(
            FlooredIntPoint(mainframe_intersection_rect.offset), IntSize());
      } else {
        mainframe_intersection = EnclosingIntRect(mainframe_intersection_rect);
      }
    }

    TransformState child_frame_to_root_frame(
        TransformState::kUnapplyInverseTransformDirection);
    if (owner_layout_object) {
      owner_layout_object->MapAncestorToLocal(
          nullptr, child_frame_to_root_frame,
          kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);
      child_frame_to_root_frame.Move(
          owner_layout_object->PhysicalContentBoxOffset());
    }
    main_frame_transform_matrix =
        child_frame_to_root_frame.AccumulatedTransform();
  } else if (occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded) {
    // If the parent LocalFrameView is throttled and out-of-date, then we can't
    // get any useful information.
    occlusion_state = FrameOcclusionState::kUnknown;
  }

  // An iframe's content is always pixel-snapped, even if the iframe element has
  // non-pixel-aligned location.
  gfx::Transform main_frame_gfx_transform =
      TransformationMatrix::ToTransform(main_frame_transform_matrix);
  main_frame_gfx_transform.RoundTranslationComponents();
  SetViewportIntersection(
      {viewport_intersection, mainframe_intersection, WebRect(),
       occlusion_state, frame.GetMainFrameViewportSize(),
       frame.GetMainFrameScrollOffset(), main_frame_gfx_transform});

  UpdateFrameVisibility(!viewport_intersection.IsEmpty());

  if (ShouldReportMainFrameIntersection()) {
    IntRect projected_rect = EnclosingIntRect(PhysicalRect::EnclosingRect(
        main_frame_transform_matrix
            .ProjectQuad(FloatRect(mainframe_intersection))
            .BoundingBox()));
    // Return <0, 0, 0, 0> if there is no area.
    if (projected_rect.IsEmpty())
      projected_rect.SetLocation(IntPoint(0, 0));
    GetFrame().Client()->OnMainFrameIntersectionChanged(projected_rect);
  }

  // We don't throttle 0x0 or display:none iframes, because in practice they are
  // sometimes used to drive UI logic.
  bool hidden_for_throttling = viewport_intersection.IsEmpty() &&
                               !FrameRect().IsEmpty() && owner_layout_object;
  bool subtree_throttled = false;
  Frame* parent_frame = GetFrame().Tree().Parent();
  if (parent_frame && parent_frame->View()) {
    subtree_throttled =
        parent_frame->View()->CanThrottleRenderingForPropagation();
  }
  UpdateRenderThrottlingStatus(hidden_for_throttling, subtree_throttled);
}

void FrameView::UpdateFrameVisibility(bool intersects_viewport) {
  blink::mojom::FrameVisibility frame_visibility;
  if (LifecycleUpdatesThrottled())
    return;
  if (IsVisible()) {
    frame_visibility =
        intersects_viewport
            ? blink::mojom::FrameVisibility::kRenderedInViewport
            : blink::mojom::FrameVisibility::kRenderedOutOfViewport;
  } else {
    frame_visibility = blink::mojom::FrameVisibility::kNotRendered;
  }
  if (frame_visibility != frame_visibility_) {
    frame_visibility_ = frame_visibility;
    VisibilityChanged(frame_visibility);
  }
}

void FrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                             bool subtree_throttled,
                                             bool recurse) {
  bool visibility_changed = (hidden_for_throttling_ || subtree_throttled_) !=
                            (hidden_for_throttling || subtree_throttled ||
                             DisplayLockedInParentFrame());
  hidden_for_throttling_ = hidden_for_throttling;
  subtree_throttled_ = subtree_throttled || DisplayLockedInParentFrame();
  if (visibility_changed)
    VisibilityForThrottlingChanged();
  if (recurse) {
    for (Frame* child = GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (FrameView* child_view = child->View()) {
        child_view->UpdateRenderThrottlingStatus(
            child_view->IsHiddenForThrottling(),
            child_view->IsAttached() && CanThrottleRenderingForPropagation(),
            true);
      }
    }
  }
}

bool FrameView::RectInParentIsStable(
    const base::TimeTicks& event_timestamp) const {
  if (event_timestamp - rect_in_parent_stable_since_ <
      base::TimeDelta::FromMilliseconds(kMinScreenRectStableTimeMs)) {
    return false;
  }
  LocalFrameView* parent = ParentFrameView();
  if (!parent)
    return true;
  return parent->RectInParentIsStable(event_timestamp);
}

}  // namespace blink
