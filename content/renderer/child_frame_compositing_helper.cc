// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "content/renderer/child_frame_compositor.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace content {

ChildFrameCompositingHelper::ChildFrameCompositingHelper(
    ChildFrameCompositor* child_frame_compositor)
    : child_frame_compositor_(child_frame_compositor) {
  DCHECK(child_frame_compositor_);
}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {
  if (crash_ui_layer_)
    crash_ui_layer_->ClearClient();
}

void ChildFrameCompositingHelper::ChildFrameGone(
    const gfx::Size& frame_size_in_dip,
    float device_scale_factor) {
  surface_id_ = viz::SurfaceId();
  device_scale_factor_ = device_scale_factor;

  crash_ui_layer_ = cc::PictureLayer::Create(this);
  crash_ui_layer_->SetMasksToBounds(true);
  crash_ui_layer_->SetIsDrawable(true);

  bool prevent_contents_opaque_changes = false;
  bool is_surface_layer = false;
  child_frame_compositor_->SetLayer(
      crash_ui_layer_, prevent_contents_opaque_changes, is_surface_layer);
}

void ChildFrameCompositingHelper::SetSurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip,
    const cc::DeadlinePolicy& deadline) {
  if (surface_id_ == surface_id)
    return;

  surface_id_ = surface_id;

  surface_layer_ = cc::SurfaceLayer::Create();
  surface_layer_->SetMasksToBounds(true);
  surface_layer_->SetSurfaceHitTestable(true);
  surface_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  surface_layer_->SetSurfaceId(surface_id, deadline);

  // TODO(lfg): Investigate if it's possible to propagate the information
  // about the child surface's opacity. https://crbug.com/629851.
  bool prevent_contents_opaque_changes = true;
  child_frame_compositor_->SetLayer(surface_layer_,
                                    prevent_contents_opaque_changes,
                                    true /* is_surface_layer */);

  UpdateVisibility(true);

  surface_layer_->SetBounds(frame_size_in_dip);
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  cc::Layer* layer = child_frame_compositor_->GetLayer();
  if (layer) {
    layer->SetIsDrawable(visible);
    layer->SetHitTestable(visible);
  }
}

gfx::Rect ChildFrameCompositingHelper::PaintableRegion() {
  DCHECK(crash_ui_layer_);
  return gfx::Rect(crash_ui_layer_->bounds());
}

scoped_refptr<cc::DisplayItemList>
ChildFrameCompositingHelper::PaintContentsToDisplayList(
    PaintingControlSetting) {
  DCHECK(crash_ui_layer_);
  auto layer_size = crash_ui_layer_->bounds();
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  display_list->StartPaint();
  display_list->push<cc::DrawColorOp>(SK_ColorGRAY, SkBlendMode::kSrc);

  SkBitmap* sad_bitmap = child_frame_compositor_->GetSadPageBitmap();
  if (sad_bitmap) {
    int paint_width = sad_bitmap->width() * device_scale_factor_;
    int paint_height = sad_bitmap->height() * device_scale_factor_;
    if (layer_size.width() >= paint_width &&
        layer_size.height() >= paint_height) {
      int x = (layer_size.width() - paint_width) / 2;
      int y = (layer_size.height() - paint_height) / 2;
      if (device_scale_factor_ != 1.f) {
        display_list->push<cc::SaveOp>();
        display_list->push<cc::TranslateOp>(x, y);
        display_list->push<cc::ScaleOp>(device_scale_factor_,
                                        device_scale_factor_);
        x = 0;
        y = 0;
      }

      auto image = cc::PaintImageBuilder::WithDefault()
                       .set_id(cc::PaintImage::GetNextId())
                       .set_image(SkImage::MakeFromBitmap(*sad_bitmap),
                                  cc::PaintImage::GetNextContentId())
                       .TakePaintImage();
      display_list->push<cc::DrawImageOp>(image, x, y, nullptr);

      if (device_scale_factor_ != 1.f)
        display_list->push<cc::RestoreOp>();
    }
  }
  display_list->EndPaintOfUnpaired(gfx::Rect(layer_size));
  display_list->Finalize();
  return display_list;
}

bool ChildFrameCompositingHelper::FillsBoundsCompletely() const {
  // Because we paint a full opaque gray background.
  return true;
}

size_t ChildFrameCompositingHelper::GetApproximateUnsharedMemoryUsage() const {
  return sizeof(*this);
}

}  // namespace content
