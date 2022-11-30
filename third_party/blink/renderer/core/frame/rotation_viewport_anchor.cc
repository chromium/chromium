// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/rotation_viewport_anchor.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {

static const float kViewportAnchorRelativeEpsilon = 0.1f;
static const int kViewportToNodeMaxRelativeArea = 2;

Node* FindNonEmptyAnchorNode(const gfx::PointF& absolute_point,
                             const gfx::Rect& view_rect,
                             EventHandler& event_handler) {
  gfx::Point point = gfx::ToFlooredPoint(absolute_point);
  HitTestLocation location(point);
  Node* node =
      event_handler
          .HitTestResultAtLocation(
              location, HitTestRequest::kReadOnly | HitTestRequest::kActive)
          .InnerNode();

  if (!node)
    return nullptr;

  // If the node bounding box is sufficiently large, make a single attempt to
  // find a smaller node; the larger the node bounds, the greater the
  // variability under resize.
  gfx::Size node_size =
      node->GetLayoutObject()
          ? node->GetLayoutObject()->AbsoluteBoundingBoxRect().size()
          : gfx::Size();
  const int max_node_area =
      view_rect.width() * view_rect.height() * kViewportToNodeMaxRelativeArea;
  if (node_size.width() * node_size.height() > max_node_area) {
    gfx::Size point_offset = gfx::ScaleToFlooredSize(
        view_rect.size(), kViewportAnchorRelativeEpsilon);
    HitTestLocation alternative_location(
        point + gfx::Vector2d(point_offset.width(), point_offset.height()));
    node = event_handler
               .HitTestResultAtLocation(
                   alternative_location,
                   HitTestRequest::kReadOnly | HitTestRequest::kActive)
               .InnerNode();
  }

  while (node &&
         (!node->GetLayoutObject() ||
          node->GetLayoutObject()->AbsoluteBoundingBoxRect().IsEmpty())) {
    node = node->parentNode();
  }

  return node;
}

void MoveToEncloseRect(gfx::Rect& outer, const gfx::RectF& inner) {
  gfx::Point minimum_position = gfx::ToCeiledPoint(
      inner.bottom_right() - gfx::Vector2dF(outer.width(), outer.height()));
  gfx::Point maximum_position = gfx::ToFlooredPoint(inner.origin());

  gfx::Point outer_origin = outer.origin();
  outer_origin.SetToMax(minimum_position);
  outer_origin.SetToMin(maximum_position);

  outer.set_origin(outer_origin);
}

void MoveIntoRect(gfx::RectF& inner, const gfx::Rect& outer) {
  gfx::PointF minimum_position = gfx::PointF(outer.origin());
  gfx::PointF maximum_position = gfx::PointF(outer.bottom_right()) -
                                 gfx::Vector2dF(inner.width(), inner.height());

  // Adjust maximumPosition to the nearest lower integer because
  // VisualViewport::maximumScrollPosition() does the same.
  // The value of minumumPosition is already adjusted since it is
  // constructed from an integer point.
  maximum_position = gfx::PointF(gfx::ToFlooredPoint(maximum_position));

  gfx::PointF inner_origin = inner.origin();
  inner_origin.SetToMax(minimum_position);
  inner_origin.SetToMin(maximum_position);

  inner.set_origin(inner_origin);
}

}  // namespace

RotationViewportAnchor::RotationViewportAnchor(
    LocalFrameView& root_frame_view,
    VisualViewport& visual_viewport,
    const gfx::PointF& anchor_in_inner_view_coords,
    PageScaleConstraintsSet& page_scale_constraints_set)
    : root_frame_view_(&root_frame_view),
      visual_viewport_(&visual_viewport),
      anchor_node_(nullptr),
      anchor_in_inner_view_coords_(anchor_in_inner_view_coords),
      page_scale_constraints_set_(&page_scale_constraints_set) {
  SetAnchor();
}

RotationViewportAnchor::~RotationViewportAnchor() {
  RestoreToAnchor();
}

void RotationViewportAnchor::SetAnchor() {
  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  DCHECK(root_frame_viewport);

  old_page_scale_factor_ = visual_viewport_->Scale();
  old_minimum_page_scale_factor_ =
      page_scale_constraints_set_->FinalConstraints().minimum_scale;

  // Save the absolute location in case we won't find the anchor node, we'll
  // fall back to that.
  visual_viewport_in_document_ =
      gfx::PointF(root_frame_viewport->VisibleContentRect().origin());

  anchor_node_ = nullptr;
  anchor_node_bounds_ = PhysicalRect();
  anchor_in_node_coords_ = gfx::PointF();
  normalized_visual_viewport_offset_ = gfx::Vector2dF();

  gfx::Rect inner_view_rect = root_frame_viewport->VisibleContentRect();

  // Preserve origins at the absolute screen origin.
  if (inner_view_rect.origin().IsOrigin() || inner_view_rect.IsEmpty())
    return;

  gfx::Rect outer_view_rect =
      LayoutViewport().VisibleContentRect(kIncludeScrollbars);

  // Normalize by the size of the outer rect
  DCHECK(!outer_view_rect.IsEmpty());
  normalized_visual_viewport_offset_ = gfx::ScaleVector2d(
      visual_viewport_->GetScrollOffset(), 1.0 / outer_view_rect.width(),
      1.0 / outer_view_rect.height());

  // Note, we specifically use the unscaled visual viewport size here as the
  // conversion into content-space below will apply the scale.
  gfx::PointF anchor_offset(visual_viewport_->Size().width(),
                            visual_viewport_->Size().height());
  anchor_offset.Scale(anchor_in_inner_view_coords_.x(),
                      anchor_in_inner_view_coords_.y());

  // Note, we specifically convert to the rootFrameView contents here, rather
  // than the layout viewport. That's because hit testing works from the
  // LocalFrameView's absolute coordinates even if it's not the layout viewport.
  const gfx::PointF anchor_point_in_document =
      root_frame_view_->RootFrameToDocument(
          visual_viewport_->ViewportToRootFrame(anchor_offset));

  Node* node = FindNonEmptyAnchorNode(
      root_frame_view_->DocumentToFrame(anchor_point_in_document),
      inner_view_rect, root_frame_view_->GetFrame().GetEventHandler());
  if (!node || !node->GetLayoutObject())
    return;

  anchor_node_ = node;
  anchor_node_bounds_ = root_frame_view_->FrameToDocument(
      PhysicalRect(node->GetLayoutObject()->AbsoluteBoundingBoxRect()));
  anchor_in_node_coords_ =
      anchor_point_in_document - gfx::Vector2dF(anchor_node_bounds_.offset);
  anchor_in_node_coords_.Scale(1.f / anchor_node_bounds_.Width(),
                               1.f / anchor_node_bounds_.Height());
}

void RotationViewportAnchor::RestoreToAnchor() {
  float new_page_scale_factor =
      old_page_scale_factor_ / old_minimum_page_scale_factor_ *
      page_scale_constraints_set_->FinalConstraints().minimum_scale;
  new_page_scale_factor =
      page_scale_constraints_set_->FinalConstraints().ClampToConstraints(
          new_page_scale_factor);

  gfx::SizeF visual_viewport_size(visual_viewport_->Size());
  visual_viewport_size.Scale(1 / new_page_scale_factor);

  gfx::Point main_frame_origin;
  gfx::PointF visual_viewport_origin;

  ComputeOrigins(visual_viewport_size, main_frame_origin,
                 visual_viewport_origin);

  LayoutViewport().SetScrollOffset(
      ScrollOffset(main_frame_origin.OffsetFromOrigin()),
      mojom::blink::ScrollType::kProgrammatic);

  // Set scale before location, since location can be clamped on setting scale.
  visual_viewport_->SetScale(new_page_scale_factor);
  visual_viewport_->SetLocation(visual_viewport_origin);
}

ScrollableArea& RotationViewportAnchor::LayoutViewport() const {
  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  DCHECK(root_frame_viewport);
  return root_frame_viewport->LayoutViewport();
}

void RotationViewportAnchor::ComputeOrigins(
    const gfx::SizeF& inner_size,
    gfx::Point& main_frame_origin,
    gfx::PointF& visual_viewport_origin) const {
  gfx::Size outer_size = LayoutViewport().VisibleContentRect().size();

  // Compute the viewport origins in CSS pixels relative to the document.
  gfx::Vector2dF abs_visual_viewport_offset =
      gfx::ScaleVector2d(normalized_visual_viewport_offset_, outer_size.width(),
                         outer_size.height());

  gfx::PointF inner_origin = GetInnerOrigin(inner_size);
  gfx::PointF outer_origin = inner_origin - abs_visual_viewport_offset;

  gfx::Rect outer_rect(gfx::ToFlooredPoint(outer_origin), outer_size);
  gfx::RectF inner_rect(inner_origin, inner_size);

  MoveToEncloseRect(outer_rect, inner_rect);

  outer_rect.set_origin(gfx::PointAtOffsetFromOrigin(
      LayoutViewport().ClampScrollOffset(outer_rect.OffsetFromOrigin())));

  MoveIntoRect(inner_rect, outer_rect);

  main_frame_origin = outer_rect.origin();
  visual_viewport_origin = inner_rect.origin() - outer_rect.OffsetFromOrigin();
}

gfx::PointF RotationViewportAnchor::GetInnerOrigin(
    const gfx::SizeF& inner_size) const {
  if (!anchor_node_ || !anchor_node_->isConnected() ||
      !anchor_node_->GetLayoutObject())
    return visual_viewport_in_document_;

  const PhysicalRect current_node_bounds = root_frame_view_->FrameToDocument(
      PhysicalRect(anchor_node_->GetLayoutObject()->AbsoluteBoundingBoxRect()));
  if (anchor_node_bounds_ == current_node_bounds)
    return visual_viewport_in_document_;

  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  const PhysicalRect current_node_bounds_in_layout_viewport =
      root_frame_viewport->RootContentsToLayoutViewportContents(
          *root_frame_view_, current_node_bounds);

  // Compute the new anchor point relative to the node position
  gfx::Vector2dF anchor_offset_from_node(
      current_node_bounds_in_layout_viewport.size.width,
      current_node_bounds_in_layout_viewport.size.height);
  anchor_offset_from_node.Scale(anchor_in_node_coords_.x(),
                                anchor_in_node_coords_.y());
  gfx::PointF anchor_point =
      gfx::PointF(current_node_bounds_in_layout_viewport.offset) +
      anchor_offset_from_node;

  // Compute the new origin point relative to the new anchor point
  gfx::Vector2dF anchor_offset_from_origin = gfx::ScaleVector2d(
      gfx::Vector2dF(inner_size.width(), inner_size.height()),
      anchor_in_inner_view_coords_.x(), anchor_in_inner_view_coords_.y());
  return anchor_point - anchor_offset_from_origin;
}

}  // namespace blink
