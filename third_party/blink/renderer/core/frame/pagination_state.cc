// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pagination_state.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

PaginationState::PaginationState()
    : content_area_paint_properties_(
          MakeGarbageCollected<ObjectPaintProperties>()) {}

void PaginationState::Trace(Visitor* visitor) const {
  visitor->Trace(anonymous_page_objects_);
  visitor->Trace(content_area_paint_properties_);
}

LayoutBlockFlow* PaginationState::CreateAnonymousPageLayoutObject(
    Document& document,
    const ComputedStyle& style) {
  LayoutBlockFlow* block = LayoutBlockFlow::CreateAnonymous(&document, &style);
  block->SetIsDetachedNonDomRoot(true);
  anonymous_page_objects_.push_back(block);
  return block;
}

void PaginationState::DestroyAnonymousPageLayoutObjects() {
  for (LayoutObject* object : anonymous_page_objects_) {
    object->Destroy();
  }
  anonymous_page_objects_.clear();
}

ObjectPaintProperties& PaginationState::EnsureContentAreaProperties(
    const TransformPaintPropertyNodeOrAlias& parent_transform,
    const ClipPaintPropertyNodeOrAlias& parent_clip) {
  // Create paint property nodes if they haven't already been created. They will
  // be initialized, and inserted between the paint properties of the LayoutView
  // and the document contents. They will be updated as each page is painted. A
  // translation node is used both to apply scaling and paint offset translation
  // (into the stitched coordinate system). A clip node is used to clip to the
  // current page area.

  if (content_area_paint_properties_->Transform()) {
    // We only need to create the property nodes once per print job, i.e. when
    // handling the first page area during pre-paint. If a transform node has
    // already been created, there should be a clip node there as well.
    DCHECK(content_area_paint_properties_->OverflowClip());
    return *content_area_paint_properties_;
  }

  // Create transform node.
  content_area_paint_properties_->UpdateTransform(
      parent_transform, TransformPaintPropertyNode::State());

  // Create clip node.
  ClipPaintPropertyNode::State clip_state(parent_transform, gfx::RectF(),
                                          FloatRoundedRect());
  content_area_paint_properties_->UpdateOverflowClip(parent_clip,
                                                     std::move(clip_state));

  return *content_area_paint_properties_;
}

void PaginationState::UpdateContentAreaPropertiesForCurrentPage(
    const LayoutView& layout_view) {
  DCHECK(layout_view.ShouldUsePaginatedLayout());
  auto chunk_properties = layout_view.FirstFragment().ContentsProperties();
  const PhysicalBoxFragment& page_container =
      *GetPageContainer(layout_view, current_page_index_);
  float scale = TargetScaleForPage(page_container);
  const PhysicalFragmentLink& link = GetPageBorderBoxLink(page_container);
  const auto& page_border_box = *To<PhysicalBoxFragment>(link.get());
  // The content rectangle is in the coordinate system of layout, i.e. with
  // layout scaling applied. Scale to target, to reverse layout scaling and to
  // apply any shrinking needed to fit the target (if there's a given paper size
  // to take into consideration).
  PhysicalRect target_content_rect = page_border_box.ContentRect();
  target_content_rect.Scale(scale);

  // The page border box offset itself is already in the target coordinate
  // system, on the other hand.
  PhysicalOffset page_border_box_offset = link.offset;
  target_content_rect.offset += page_border_box_offset;

  gfx::Transform matrix;

  // Translate by the distance from the top/left page box (paper) corner to the
  // top/left corner of the page content area, in the target coordinate system.
  matrix.Translate(float(target_content_rect.offset.left),
                   float(target_content_rect.offset.top));

  // Transform into the coordinate system used by layout.
  matrix.Scale(scale);

  // Translate by the offset into the stitched coordinate system for the given
  // page.
  PhysicalOffset stitched_offset =
      StitchedPageContentRect(layout_view, current_page_index_).offset;
  matrix.Translate(-gfx::Vector2dF(stitched_offset));

  TransformPaintPropertyNode::State transform_state;
  transform_state.transform_and_origin = {matrix, gfx::Point3F()};

  content_area_paint_properties_->UpdateTransform(chunk_properties.Transform(),
                                                  std::move(transform_state));

  // Clip to the current page area. When printing one page (the current page),
  // all pages that have overflowing content into the current page also need to
  // be painted, to encompass overflow (content from one page may overflow into
  // other pages, e.g. via relative positioning, or monolithic overflow).
  gfx::RectF target_page_area_rect(gfx::PointF(target_content_rect.offset),
                                   gfx::SizeF(target_content_rect.size));
  ClipPaintPropertyNode::State clip_state(
      chunk_properties.Transform(), target_page_area_rect,
      FloatRoundedRect(target_page_area_rect));
  content_area_paint_properties_->UpdateOverflowClip(chunk_properties.Clip(),
                                                     std::move(clip_state));
}

PropertyTreeState PaginationState::ContentAreaPropertyTreeStateForCurrentPage(
    const LayoutView& layout_view) const {
  DCHECK(content_area_paint_properties_);
  const EffectPaintPropertyNode& effect_node =
      layout_view.FirstFragment().ContentsEffect().Unalias();

  return PropertyTreeState(*content_area_paint_properties_->Transform(),
                           *content_area_paint_properties_->OverflowClip(),
                           effect_node);
}

}  // namespace blink
