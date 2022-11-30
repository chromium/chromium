// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/child_frame_compositing_helper.h"

#include <utility>

#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/renderer/core/frame/child_frame_compositor.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

ChildFrameCompositingHelper::ChildFrameCompositingHelper(
    ChildFrameCompositor* child_frame_compositor)
    : child_frame_compositor_(child_frame_compositor) {
  DCHECK(child_frame_compositor_);
}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {
  if (crash_ui_layer_)
    crash_ui_layer_->ClearClient();
}

void ChildFrameCompositingHelper::ChildFrameGone(float device_scale_factor) {
  surface_id_ = viz::SurfaceId();
  device_scale_factor_ = device_scale_factor;

  crash_ui_layer_ = cc::PictureLayer::Create(this);
  crash_ui_layer_->SetMasksToBounds(true);
  crash_ui_layer_->SetIsDrawable(true);

  bool is_surface_layer = false;
  child_frame_compositor_->SetCcLayer(crash_ui_layer_, is_surface_layer);
}

void ChildFrameCompositingHelper::SetSurfaceId(
    const viz::SurfaceId& surface_id,
    bool capture_sequence_number_changed) {
  if (surface_id_ == surface_id)
    return;

  surface_id_ = surface_id;

  surface_layer_ = cc::SurfaceLayer::Create();
  surface_layer_->SetMasksToBounds(true);
  surface_layer_->SetSurfaceHitTestable(true);
  surface_layer_->SetBackgroundColor(SkColors::kTransparent);

  // If we're synchronizing surfaces, then use an infinite deadline to ensure
  // everything is synchronized.
  cc::DeadlinePolicy deadline = capture_sequence_number_changed
                                    ? cc::DeadlinePolicy::UseInfiniteDeadline()
                                    : cc::DeadlinePolicy::UseDefaultDeadline();
  surface_layer_->SetSurfaceId(surface_id, deadline);

  // TODO(lfg): Investigate if it's possible to propagate the information
  // about the child surface's opacity. https://crbug.com/629851.
  child_frame_compositor_->SetCcLayer(surface_layer_,
                                      true /* is_surface_layer */);

  UpdateVisibility(true);
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  const scoped_refptr<cc::Layer>& layer = child_frame_compositor_->GetCcLayer();
  if (layer) {
    layer->SetIsDrawable(visible);
    layer->SetHitTestable(visible);
  }
}

gfx::Rect ChildFrameCompositingHelper::PaintableRegion() const {
  DCHECK(crash_ui_layer_);
  return gfx::Rect(crash_ui_layer_->bounds());
}

scoped_refptr<cc::DisplayItemList>
ChildFrameCompositingHelper::PaintContentsToDisplayList() {
  DCHECK(crash_ui_layer_);
  auto layer_size = crash_ui_layer_->bounds();
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  display_list->StartPaint();
  display_list->push<cc::DrawColorOp>(SkColors::kGray, SkBlendMode::kSrc);

  SkBitmap* sad_bitmap = child_frame_compositor_->GetSadPageBitmap();
  if (sad_bitmap) {
    float paint_width = sad_bitmap->width() * device_scale_factor_;
    float paint_height = sad_bitmap->height() * device_scale_factor_;
    if (layer_size.width() >= paint_width &&
        layer_size.height() >= paint_height) {
      float x = (layer_size.width() - paint_width) / 2.0f;
      float y = (layer_size.height() - paint_height) / 2.0f;
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
      display_list->push<cc::DrawImageOp>(image, x, y);

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

}  // namespace blink
