// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_view.h"

#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remote_frame)
    : FrameView(IntRect()), remote_frame_(remote_frame) {
  DCHECK(remote_frame);
  Show();
}

RemoteFrameView::~RemoteFrameView() = default;

LocalFrameView* RemoteFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner && owner->OwnerType() == FrameOwnerElementType::kPortal)
    return owner->GetDocument().GetFrame()->View();

  // |is_attached_| is only set from AttachToLayout(), which ensures that the
  // parent is a local frame.
  return To<LocalFrame>(remote_frame_->Tree().Parent())->View();
}

LayoutEmbeddedContent* RemoteFrameView::GetLayoutEmbeddedContent() const {
  return remote_frame_->OwnerLayoutObject();
}

LocalFrameView* RemoteFrameView::ParentLocalRootFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner && owner->OwnerType() == FrameOwnerElementType::kPortal)
    return owner->GetDocument().GetFrame()->LocalFrameRoot().View();

  // |is_attached_| is only set from AttachToLayout(), which ensures that the
  // parent is a local frame.
  return To<LocalFrame>(remote_frame_->Tree().Parent())
      ->LocalFrameRoot()
      .View();
}

void RemoteFrameView::AttachToLayout() {
  DCHECK(!IsAttached());
  SetAttached(true);
  if (ParentFrameView()->IsVisible())
    SetParentVisible(true);
  UpdateFrameVisibility(true);
  UpdateRenderThrottlingStatus(
      IsHiddenForThrottling(),
      ParentFrameView()->CanThrottleRenderingForPropagation());
  FrameRectsChanged(FrameRect());
}

void RemoteFrameView::DetachFromLayout() {
  DCHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);
}

bool RemoteFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags) {
  UpdateViewportIntersection(parent_flags, needs_occlusion_tracking_);
  return needs_occlusion_tracking_;
}

void RemoteFrameView::SetViewportIntersection(
    const ViewportIntersectionState& intersection_state) {
  if (intersection_state != last_intersection_state_) {
    last_intersection_state_ = intersection_state;
    remote_frame_->Client()->UpdateRemoteViewportIntersection(
        intersection_state);
  }
}

void RemoteFrameView::SetNeedsOcclusionTracking(bool needs_tracking) {
  if (needs_occlusion_tracking_ == needs_tracking)
    return;
  needs_occlusion_tracking_ = needs_tracking;
  if (needs_tracking) {
    if (LocalFrameView* parent_view = ParentLocalRootFrameView())
      parent_view->ScheduleAnimation();
  }
}

IntRect RemoteFrameView::GetCompositingRect() {
  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  if (!local_root_view || !remote_frame_->OwnerLayoutObject())
    return IntRect();

  // For main frames we constrain the rect that gets painted to the viewport.
  // If the local frame root is an OOPIF itself, then we use the root's
  // intersection rect. This represents a conservative maximum for the area
  // that needs to be rastered by the OOPIF compositor.
  IntSize viewport_size = local_root_view->Size();
  if (local_root_view->GetPage()->MainFrame() != local_root_view->GetFrame()) {
    viewport_size =
        local_root_view->GetFrame().RemoteViewportIntersection().Size();
  }

  // The viewport size needs to account for intermediate CSS transforms before
  // being compared to the frame size.
  PhysicalRect viewport_rect =
      remote_frame_->OwnerLayoutObject()->AncestorToLocalRect(
          local_root_view->GetLayoutView(),
          PhysicalRect(PhysicalOffset(), PhysicalSize(viewport_size)),
          kTraverseDocumentBoundaries);
  IntSize converted_viewport_size = EnclosingIntRect(viewport_rect).Size();

  IntSize frame_size = Size();

  // Iframes that fit within the window viewport get fully rastered. For
  // iframes that are larger than the window viewport, add a 30% buffer to the
  // draw area to try to prevent guttering during scroll.
  // TODO(kenrb): The 30% value is arbitrary, it gives 15% overdraw in both
  // directions when the iframe extends beyond both edges of the viewport, and
  // it seems to make guttering rare with slow to medium speed wheel scrolling.
  // Can we collect UMA data to estimate how much extra rastering this causes,
  // and possibly how common guttering is?
  converted_viewport_size.Scale(1.3f);
  converted_viewport_size.SetWidth(
      std::min(frame_size.Width(), converted_viewport_size.Width()));
  converted_viewport_size.SetHeight(
      std::min(frame_size.Height(), converted_viewport_size.Height()));
  IntPoint expanded_origin;
  const IntRect& last_rect = last_intersection_state_.viewport_intersection;
  if (!last_rect.IsEmpty()) {
    IntSize expanded_size =
        last_rect.Size().ExpandedTo(converted_viewport_size);
    expanded_size -= last_rect.Size();
    expanded_size.Scale(0.5f, 0.5f);
    expanded_origin = last_rect.Location() - expanded_size;
    expanded_origin.ClampNegativeToZero();
  }
  return IntRect(expanded_origin, converted_viewport_size);
}

void RemoteFrameView::Dispose() {
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  // ownerElement can be null during frame swaps, because the
  // RemoteFrameView is disconnected before detachment.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);
  SetNeedsOcclusionTracking(false);
}

void RemoteFrameView::InvalidateRect(const IntRect& rect) {
  auto* object = remote_frame_->OwnerLayoutObject();
  if (!object)
    return;

  PhysicalRect repaint_rect(rect);
  repaint_rect.Move(PhysicalOffset(object->BorderLeft() + object->PaddingLeft(),
                                   object->BorderTop() + object->PaddingTop()));
  object->InvalidatePaintRectangle(repaint_rect);
}

void RemoteFrameView::PropagateFrameRects() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  IntRect frame_rect(FrameRect());
  IntRect screen_space_rect = frame_rect;

  if (LocalFrameView* parent = ParentFrameView()) {
    screen_space_rect = parent->ConvertToRootFrame(screen_space_rect);
  }
  remote_frame_->Client()->FrameRectsChanged(frame_rect, screen_space_rect);
}

void RemoteFrameView::Paint(GraphicsContext& context,
                            const GlobalPaintFlags flags,
                            const CullRect& rect,
                            const IntSize& paint_offset) const {
  // Painting remote frames is only for printing.
  if (!context.Printing())
    return;

  if (!rect.Intersects(FrameRect()))
    return;

  DrawingRecorder recorder(context, *GetFrame().OwnerLayoutObject(),
                           DisplayItem::kDocumentBackground);
  context.Save();
  context.Translate(paint_offset.Width(), paint_offset.Height());

  DCHECK(context.Canvas());
  // Inform the remote frame to print.
  uint32_t content_id = Print(FrameRect(), context.Canvas());

  // Record the place holder id on canvas.
  context.Canvas()->recordCustomData(content_id);
  context.Restore();
}

void RemoteFrameView::UpdateGeometry() {
  if (LayoutEmbeddedContent* layout = GetLayoutEmbeddedContent())
    layout->UpdateGeometry(*this);
}

void RemoteFrameView::Hide() {
  SetSelfVisible(false);
  UpdateFrameVisibility(
      !last_intersection_state_.viewport_intersection.IsEmpty());
}

void RemoteFrameView::Show() {
  SetSelfVisible(true);
  UpdateFrameVisibility(
      !last_intersection_state_.viewport_intersection.IsEmpty());
}

void RemoteFrameView::ParentVisibleChanged() {
  if (IsSelfVisible()) {
    UpdateFrameVisibility(
        !last_intersection_state_.viewport_intersection.IsEmpty());
  }
}

void RemoteFrameView::VisibilityForThrottlingChanged() {
  TRACE_EVENT0("blink", "RemoteFrameView::VisibilityForThrottlingChanged");
  if (!remote_frame_->Client())
    return;
  remote_frame_->Client()->UpdateRenderThrottlingStatus(IsHiddenForThrottling(),
                                                        IsSubtreeThrottled());
}

void RemoteFrameView::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  remote_frame_->GetRemoteFrameHostRemote().VisibilityChanged(visibility);
}

bool RemoteFrameView::CanThrottleRendering() const {
  return IsSubtreeThrottled() || IsHiddenForThrottling();
}

void RemoteFrameView::SetIntrinsicSizeInfo(
    const IntrinsicSizingInfo& size_info) {
  intrinsic_sizing_info_ = size_info;
  has_intrinsic_sizing_info_ = true;
}

bool RemoteFrameView::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& sizing_info) const {
  if (!has_intrinsic_sizing_info_)
    return false;

  sizing_info = intrinsic_sizing_info_;
  return true;
}

bool RemoteFrameView::HasIntrinsicSizingInfo() const {
  return has_intrinsic_sizing_info_;
}

uint32_t RemoteFrameView::Print(const IntRect& rect,
                                cc::PaintCanvas* canvas) const {
  return remote_frame_->Client()->Print(rect, canvas);
}

void RemoteFrameView::Trace(blink::Visitor* visitor) {
  visitor->Trace(remote_frame_);
}

}  // namespace blink
