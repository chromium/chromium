// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_view.h"

#include <algorithm>

#include "components/paint_preview/common/paint_preview_tracker.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"

#if BUILDFLAG(ENABLE_PRINTING)
// nogncheck because dependency on //printing is conditional upon
// enable_printing flags.
#include "printing/metafile_skia.h"  // nogncheck
#endif

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remote_frame)
    : FrameView(gfx::Rect()), remote_frame_(remote_frame) {
  DCHECK(remote_frame);
  Show();
}

RemoteFrameView::~RemoteFrameView() = default;

LocalFrameView* RemoteFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner && owner->OwnerType() == FrameOwnerElementType::kFencedframe) {
    return owner->GetDocument().GetFrame()->View();
  }

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
  if (owner && owner->OwnerType() == FrameOwnerElementType::kFencedframe) {
    return owner->GetDocument().GetFrame()->LocalFrameRoot().View();
  }

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
      ParentFrameView()->CanThrottleRenderingForPropagation(),
      IsDisplayLocked());
  needs_frame_rect_propagation_ = true;
  ParentFrameView()->SetNeedsUpdateGeometries();
}

void RemoteFrameView::DetachFromLayout() {
  DCHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);
}

bool RemoteFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags,
    ComputeIntersectionsContext&) {
  UpdateViewportIntersection(parent_flags, needs_occlusion_tracking_);
  return needs_occlusion_tracking_;
}

void RemoteFrameView::SetViewportIntersection(
    const mojom::blink::ViewportIntersectionState& intersection_state) {
  mojom::blink::ViewportIntersectionState new_state(intersection_state);
  new_state.compositor_visible_rect = compositing_rect_;
  if (!last_intersection_state_.Equals(new_state)) {
    last_intersection_state_ = new_state;
    remote_frame_->SetViewportIntersection(new_state);
  } else if (needs_frame_rect_propagation_) {
    PropagateFrameRects();
  }
}

void RemoteFrameView::SetNeedsOcclusionTracking(bool needs_tracking) {
  if (needs_occlusion_tracking_ == needs_tracking)
    return;
  needs_occlusion_tracking_ = needs_tracking;
  if (needs_tracking) {
    if (LocalFrameView* parent_view = ParentLocalRootFrameView()) {
      parent_view->SetIntersectionObservationState(LocalFrameView::kRequired);
      parent_view->ScheduleAnimation();
    }
  }
}

gfx::Rect RemoteFrameView::ComputeCompositingRect() const {
  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  LayoutEmbeddedContent* owner_layout_object =
      remote_frame_->OwnerLayoutObject();

  // For main frames we constrain the rect that gets painted to the viewport.
  // If the local frame root is an OOPIF itself, then we use the root's
  // intersection rect. This represents a conservative maximum for the area
  // that needs to be rastered by the OOPIF compositor.
  gfx::Rect viewport_rect(gfx::Point(), local_root_view->Size());
  if (local_root_view->GetPage()->MainFrame() != local_root_view->GetFrame()) {
    viewport_rect = local_root_view->GetFrame().RemoteViewportIntersection();
  }

  // The viewport rect needs to account for intermediate CSS transforms before
  // being compared to the frame size.
  TransformState local_root_transform_state(
      TransformState::kApplyTransformDirection);
  local_root_transform_state.Move(
      owner_layout_object->PhysicalContentBoxOffset());
  owner_layout_object->MapLocalToAncestor(nullptr, local_root_transform_state,
                                          kTraverseDocumentBoundaries);
  gfx::Transform matrix =
      local_root_transform_state.AccumulatedTransform().InverseOrIdentity();
  PhysicalRect local_viewport_rect = PhysicalRect::EnclosingRect(
      matrix.ProjectQuad(gfx::QuadF(gfx::RectF(viewport_rect))).BoundingBox());
  gfx::Rect compositing_rect = ToEnclosingRect(local_viewport_rect);
  gfx::Size frame_size = Size();

  // Iframes that fit within the window viewport get fully rastered. For
  // iframes that are larger than the window viewport, add a 30% buffer to the
  // draw area to try to prevent guttering during scroll.
  // TODO(kenrb): The 30% value is arbitrary, it gives 15% overdraw in both
  // directions when the iframe extends beyond both edges of the viewport, and
  // it seems to make guttering rare with slow to medium speed wheel scrolling.
  // Can we collect UMA data to estimate how much extra rastering this causes,
  // and possibly how common guttering is?
  compositing_rect.Outset(
      gfx::Outsets::VH(ceilf(local_viewport_rect.Height() * 0.15f),
                       ceilf(local_viewport_rect.Width() * 0.15f)));
  compositing_rect.set_width(
      std::min(frame_size.width(), compositing_rect.width()));
  compositing_rect.set_height(
      std::min(frame_size.height(), compositing_rect.height()));
  gfx::Point compositing_rect_location = compositing_rect.origin();
  compositing_rect_location.SetToMax(gfx::Point());
  compositing_rect.set_origin(compositing_rect_location);

  return compositing_rect;
}

void RemoteFrameView::UpdateCompositingRect() {
  remote_frame_->UpdateCompositedLayerBounds();
  gfx::Rect previous_rect = compositing_rect_;
  compositing_rect_ = gfx::Rect();
  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  LayoutEmbeddedContent* owner_layout_object =
      remote_frame_->OwnerLayoutObject();
  if (!local_root_view || !owner_layout_object) {
    needs_frame_rect_propagation_ = true;
    return;
  }

  // The |compositing_rect_| provides the child compositor the rectangle (in its
  // local coordinate space) which should be rasterized/composited. Its based on
  // the child frame's intersection with the viewport and an optimization to
  // avoid large iframes rasterizing their complete viewport.
  // Since this rectangle is dependent on the child frame's position in the
  // embedding frame, updating this can be used for communication with a fenced
  // frame. So if the frame size is frozen, we use the complete viewport of the
  // child frame as its compositing rect.
  if (frozen_size_) {
    compositing_rect_ = gfx::Rect(*frozen_size_);
  } else {
    compositing_rect_ = ComputeCompositingRect();
  }

  if (compositing_rect_ != previous_rect)
    needs_frame_rect_propagation_ = true;
}

void RemoteFrameView::UpdateCompositingScaleFactor() {
  float previous_scale_factor = compositing_scale_factor_;

  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  LayoutEmbeddedContent* owner_layout_object =
      remote_frame_->OwnerLayoutObject();
  if (!local_root_view || !owner_layout_object)
    return;

  TransformState local_root_transform_state(
      TransformState::kApplyTransformDirection);
  local_root_transform_state.Move(
      owner_layout_object->PhysicalContentBoxOffset());
  owner_layout_object->MapLocalToAncestor(nullptr, local_root_transform_state,
                                          kTraverseDocumentBoundaries);

  float frame_to_local_root_scale_factor = 1.0f;
  gfx::Transform local_root_transform =
      local_root_transform_state.AccumulatedTransform();
  std::optional<gfx::Vector2dF> scale_components =
      gfx::TryComputeTransform2dScaleComponents(local_root_transform);
  if (!scale_components) {
    frame_to_local_root_scale_factor =
        gfx::ComputeApproximateMaxScale(local_root_transform);
  } else {
    frame_to_local_root_scale_factor =
        std::max(scale_components->x(), scale_components->y());
  }

  // The compositing scale factor is calculated by multiplying the scale factor
  // from the local root to main frame with the scale factor between child frame
  // and local root.
  FrameWidget* widget = local_root_view->GetFrame().GetWidgetForLocalRoot();
  compositing_scale_factor_ =
      widget->GetCompositingScaleFactor() * frame_to_local_root_scale_factor;

  // Force compositing scale factor to be within reasonable minimum and maximum
  // values to prevent dependent values such as scroll deltas in the compositor
  // going to zero or extremely high memory usage due to large raster scales.
  // It's possible for the calculated scale factor to become very large or very
  // small since it depends on arbitrary intermediate CSS transforms.
  constexpr float kMinCompositingScaleFactor = 0.25f;
  constexpr float kMaxCompositingScaleFactor = 5.0f;
  compositing_scale_factor_ =
      std::clamp(compositing_scale_factor_, kMinCompositingScaleFactor,
                 kMaxCompositingScaleFactor);

  if (compositing_scale_factor_ != previous_scale_factor)
    remote_frame_->SynchronizeVisualProperties();
}

void RemoteFrameView::Dispose() {
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  // ownerElement can be null during frame swaps, because the
  // RemoteFrameView is disconnected before detachment.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);
}

void RemoteFrameView::SetFrameRect(const gfx::Rect& rect) {
  UpdateFrozenSize();
  EmbeddedContentView::SetFrameRect(rect);
  if (needs_frame_rect_propagation_)
    PropagateFrameRects();
}

void RemoteFrameView::UpdateFrozenSize() {
  if (frozen_size_)
    return;
  auto* layout_embedded_content = GetLayoutEmbeddedContent();
  if (!layout_embedded_content)
    return;
  std::optional<PhysicalSize> frozen_phys_size =
      layout_embedded_content->FrozenFrameSize();
  if (!frozen_phys_size)
    return;
  const gfx::Size rounded_frozen_size(frozen_phys_size->width.Ceil(),
                                      frozen_phys_size->height.Ceil());
  frozen_size_ = rounded_frozen_size;
  needs_frame_rect_propagation_ = true;
}

void RemoteFrameView::ZoomFactorChanged(float zoom_factor) {
  remote_frame_->ZoomFactorChanged(zoom_factor);
}

void RemoteFrameView::PropagateFrameRects() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  needs_frame_rect_propagation_ = false;
  gfx::Rect frame_rect(FrameRect());
  gfx::Rect rect_in_local_root = frame_rect;

  if (LocalFrameView* parent = ParentFrameView()) {
    rect_in_local_root = parent->ConvertToRootFrame(rect_in_local_root);
  }

  gfx::Size frame_size = frozen_size_.value_or(frame_rect.size());
  remote_frame_->FrameRectsChanged(frame_size, rect_in_local_root);
}

void RemoteFrameView::Paint(GraphicsContext& context,
                            PaintFlags flags,
                            const CullRect& rect,
                            const gfx::Vector2d& paint_offset) const {
  if (!rect.Intersects(FrameRect()))
    return;

  const auto& owner_layout_object = *GetFrame().OwnerLayoutObject();
  if (owner_layout_object.GetDocument().IsPrintingOrPaintingPreview()) {
    DrawingRecorder recorder(context, owner_layout_object,
                             DisplayItem::kDocumentBackground);
    context.Save();
    context.Translate(paint_offset.x(), paint_offset.y());
    DCHECK(context.Canvas());

    uint32_t content_id = 0;
    if (owner_layout_object.GetDocument().Printing()) {
      // Inform the remote frame to print.
      content_id = Print(FrameRect(), context.Canvas());
    } else {
      DCHECK_NE(Document::kNotPaintingPreview,
                owner_layout_object.GetDocument().GetPaintPreviewState());
      // Inform the remote frame to capture a paint preview.
      content_id = CapturePaintPreview(FrameRect(), context.Canvas());
    }
    // Record the place holder id on canvas.
    context.Canvas()->recordCustomData(content_id);
    context.Restore();
  }

  if (GetFrame().GetCcLayer()) {
    RecordForeignLayer(
        context, owner_layout_object, DisplayItem::kForeignLayerRemoteFrame,
        GetFrame().GetCcLayer(), FrameRect().origin() + paint_offset);
  }
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
  // TODO(szager,vmpstr): Send IsSubtreeThrottled() and IsDisplayLocked() as
  // separate bits.
  remote_frame_->GetRemoteFrameHostRemote().UpdateRenderThrottlingStatus(
      IsHiddenForThrottling(), IsSubtreeThrottled(), IsDisplayLocked());
}

void RemoteFrameView::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  remote_frame_->GetRemoteFrameHostRemote().VisibilityChanged(visibility);
}

bool RemoteFrameView::CanThrottleRendering() const {
  return IsHiddenForThrottling() || IsSubtreeThrottled() || IsDisplayLocked();
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

uint32_t RemoteFrameView::Print(const gfx::Rect& rect,
                                cc::PaintCanvas* canvas) const {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* metafile = canvas->GetPrintingMetafile();
  DCHECK(metafile);

  // Create a place holder content for the remote frame so it can be replaced
  // with actual content later.
  // TODO(crbug.com/1093929): Consider to use an embedding token which
  // represents the state of the remote frame. See also comments on
  // https://crrev.com/c/2245430/.
  uint32_t content_id = metafile->CreateContentForRemoteFrame(
      rect, remote_frame_->GetFrameToken().value());

  // Inform browser to print the remote subframe.
  remote_frame_->GetRemoteFrameHostRemote().PrintCrossProcessSubframe(
      rect, metafile->GetDocumentCookie());
  return content_id;
#else
  return 0;
#endif
}

uint32_t RemoteFrameView::CapturePaintPreview(const gfx::Rect& rect,
                                              cc::PaintCanvas* canvas) const {
  auto* tracker = canvas->GetPaintPreviewTracker();
  DCHECK(tracker);  // |tracker| must exist or there is a bug upstream.

  // RACE: there is a possibility that the embedding token will be null and
  // still be in a valid state. This can occur is the frame has recently
  // navigated and the embedding token hasn't propagated from the FrameTreeNode
  // to this HTMLFrameOwnerElement yet (over IPC). If the token is null the
  // failure can be handled gracefully by simply ignoring the subframe in the
  // result.
  std::optional<base::UnguessableToken> maybe_embedding_token =
      remote_frame_->GetEmbeddingToken();
  if (!maybe_embedding_token.has_value())
    return 0;
  uint32_t content_id =
      tracker->CreateContentForRemoteFrame(rect, maybe_embedding_token.value());

  // Send a request to the browser to trigger a capture of the remote frame.
  remote_frame_->GetRemoteFrameHostRemote()
      .CapturePaintPreviewOfCrossProcessSubframe(rect, tracker->Guid());
  return content_id;
}

void RemoteFrameView::Trace(Visitor* visitor) const {
  visitor->Trace(remote_frame_);
}

}  // namespace blink
