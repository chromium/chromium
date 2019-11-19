// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/geometry/double_rect.h"

namespace blink {

namespace {

static const float kViewportAnchorRelativeEpsilon = 0.1f;
static const int kViewportToNodeMaxRelativeArea = 2;

Node* FindNonEmptyAnchorNode(const FloatPoint& absolute_point,
                             const IntRect& view_rect,
                             EventHandler& event_handler) {
  IntPoint point = FlooredIntPoint(absolute_point);
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
  IntSize node_size =
      node->GetLayoutObject()
          ? node->GetLayoutObject()->AbsoluteBoundingBoxRect().Size()
          : IntSize();
  const int max_node_area =
      view_rect.Width() * view_rect.Height() * kViewportToNodeMaxRelativeArea;
  if (node_size.Width() * node_size.Height() > max_node_area) {
    IntSize point_offset = view_rect.Size();
    point_offset.Scale(kViewportAnchorRelativeEpsilon);
    HitTestLocation alternative_location(point + point_offset);
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

void MoveToEncloseRect(IntRect& outer, const FloatRect& inner) {
  IntPoint minimum_position =
      CeiledIntPoint(inner.Location() + inner.Size() - FloatSize(outer.Size()));
  IntPoint maximum_position = FlooredIntPoint(inner.Location());

  IntPoint outer_origin = outer.Location();
  outer_origin = outer_origin.ExpandedTo(minimum_position);
  outer_origin = outer_origin.ShrunkTo(maximum_position);

  outer.SetLocation(outer_origin);
}

void MoveIntoRect(FloatRect& inner, const IntRect& outer) {
  FloatPoint minimum_position = FloatPoint(outer.Location());
  FloatPoint maximum_position = minimum_position + outer.Size() - inner.Size();

  // Adjust maximumPosition to the nearest lower integer because
  // VisualViewport::maximumScrollPosition() does the same.
  // The value of minumumPosition is already adjusted since it is
  // constructed from an integer point.
  maximum_position = FloatPoint(FlooredIntPoint(maximum_position));

  FloatPoint inner_origin = inner.Location();
  inner_origin = inner_origin.ExpandedTo(minimum_position);
  inner_origin = inner_origin.ShrunkTo(maximum_position);

  inner.SetLocation(inner_origin);
}

}  // namespace

RotationViewportAnchor::RotationViewportAnchor(
    LocalFrameView& root_frame_view,
    VisualViewport& visual_viewport,
    const FloatSize& anchor_in_inner_view_coords,
    PageScaleConstraintsSet& page_scale_constraints_set)
    : root_frame_view_(&root_frame_view),
      visual_viewport_(&visual_viewport),
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
      FloatPoint(root_frame_viewport->VisibleContentRect().Location());

  anchor_node_.Clear();
  anchor_node_bounds_ = PhysicalRect();
  anchor_in_node_coords_ = FloatSize();
  normalized_visual_viewport_offset_ = FloatSize();

  IntRect inner_view_rect = root_frame_viewport->VisibleContentRect();

  // Preserve origins at the absolute screen origin.
  if (inner_view_rect.Location() == IntPoint::Zero() ||
      inner_view_rect.IsEmpty())
    return;

  IntRect outer_view_rect =
      LayoutViewport().VisibleContentRect(kIncludeScrollbars);

  // Normalize by the size of the outer rect
  DCHECK(!outer_view_rect.IsEmpty());
  normalized_visual_viewport_offset_ = visual_viewport_->GetScrollOffset();
  normalized_visual_viewport_offset_.Scale(1.0 / outer_view_rect.Width(),
                                           1.0 / outer_view_rect.Height());

  // Note, we specifically use the unscaled visual viewport size here as the
  // conversion into content-space below will apply the scale.
  FloatPoint anchor_offset(visual_viewport_->Size());
  anchor_offset.Scale(anchor_in_inner_view_coords_.Width(),
                      anchor_in_inner_view_coords_.Height());

  // Note, we specifically convert to the rootFrameView contents here, rather
  // than the layout viewport. That's because hit testing works from the
  // LocalFrameView's absolute coordinates even if it's not the layout viewport.
  const FloatPoint anchor_point_in_document =
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
      anchor_point_in_document - FloatPoint(anchor_node_bounds_.offset);
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

  FloatSize visual_viewport_size(visual_viewport_->Size());
  visual_viewport_size.Scale(1 / new_page_scale_factor);

  IntPoint main_frame_origin;
  FloatPoint visual_viewport_origin;

  ComputeOrigins(visual_viewport_size, main_frame_origin,
                 visual_viewport_origin);

  LayoutViewport().SetScrollOffset(
      ToScrollOffset(FloatPoint(main_frame_origin)), kProgrammaticScroll);

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
    const FloatSize& inner_size,
    IntPoint& main_frame_offset,
    FloatPoint& visual_viewport_offset) const {
  IntSize outer_size = LayoutViewport().VisibleContentRect().Size();

  // Compute the viewport origins in CSS pixels relative to the document.
  FloatSize abs_visual_viewport_offset = normalized_visual_viewport_offset_;
  abs_visual_viewport_offset.Scale(outer_size.Width(), outer_size.Height());

  FloatPoint inner_origin = GetInnerOrigin(inner_size);
  FloatPoint outer_origin = inner_origin - abs_visual_viewport_offset;

  IntRect outer_rect = IntRect(FlooredIntPoint(outer_origin), outer_size);
  FloatRect inner_rect = FloatRect(inner_origin, inner_size);

  MoveToEncloseRect(outer_rect, inner_rect);

  outer_rect.SetLocation(IntPoint(
      LayoutViewport().ClampScrollOffset(ToIntSize(outer_rect.Location()))));

  MoveIntoRect(inner_rect, outer_rect);

  main_frame_offset = outer_rect.Location();
  visual_viewport_offset =
      FloatPoint(inner_rect.Location() - outer_rect.Location());
}

FloatPoint RotationViewportAnchor::GetInnerOrigin(
    const FloatSize& inner_size) const {
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
          *root_frame_view_.Get(), current_node_bounds);

  // Compute the new anchor point relative to the node position
  FloatSize anchor_offset_from_node(
      current_node_bounds_in_layout_viewport.size);
  anchor_offset_from_node.Scale(anchor_in_node_coords_.Width(),
                                anchor_in_node_coords_.Height());
  FloatPoint anchor_point =
      FloatPoint(current_node_bounds_in_layout_viewport.offset) +
      anchor_offset_from_node;

  // Compute the new origin point relative to the new anchor point
  FloatSize anchor_offset_from_origin = inner_size;
  anchor_offset_from_origin.Scale(anchor_in_inner_view_coords_.Width(),
                                  anchor_in_inner_view_coords_.Height());
  return anchor_point - anchor_offset_from_origin;
}

}  // namespace blink
