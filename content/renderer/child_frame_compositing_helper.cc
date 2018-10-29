// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "build/build_config.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/solid_color_layer.h"
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

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() = default;

void ChildFrameCompositingHelper::ChildFrameGone(
    const gfx::Size& frame_size_in_dip,
    float device_scale_factor) {
  primary_surface_id_ = viz::SurfaceId();
  fallback_surface_id_ = viz::SurfaceId();

  scoped_refptr<cc::SolidColorLayer> crashed_layer =
      cc::SolidColorLayer::Create();
  crashed_layer->SetMasksToBounds(true);
  crashed_layer->SetBackgroundColor(SK_ColorGRAY);

  if (child_frame_compositor_->GetLayer()) {
    SkBitmap* sad_bitmap = child_frame_compositor_->GetSadPageBitmap();
    if (sad_bitmap && frame_size_in_dip.width() > sad_bitmap->width() &&
        frame_size_in_dip.height() > sad_bitmap->height()) {
      scoped_refptr<cc::PictureImageLayer> sad_layer =
          cc::PictureImageLayer::Create();
      sad_layer->SetImage(cc::PaintImageBuilder::WithDefault()
                              .set_id(cc::PaintImage::GetNextId())
                              .set_image(SkImage::MakeFromBitmap(*sad_bitmap),
                                         cc::PaintImage::GetNextContentId())
                              .TakePaintImage(),
                          SkMatrix::I(), false);
      sad_layer->SetBounds(
          gfx::Size(sad_bitmap->width() * device_scale_factor,
                    sad_bitmap->height() * device_scale_factor));
      sad_layer->SetPosition(
          gfx::PointF((frame_size_in_dip.width() - sad_bitmap->width()) / 2,
                      (frame_size_in_dip.height() - sad_bitmap->height()) / 2));
      sad_layer->SetIsDrawable(true);

      crashed_layer->AddChild(sad_layer);
    }
  }

  bool prevent_contents_opaque_changes = false;
  child_frame_compositor_->SetLayer(std::move(crashed_layer),
                                    prevent_contents_opaque_changes,
                                    false /* is_surface_layer */);
}

void ChildFrameCompositingHelper::SetSurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip,
    const cc::DeadlinePolicy& deadline) {
  if (primary_surface_id_ == surface_id)
    return;

  primary_surface_id_ = surface_id;

  surface_layer_ = cc::SurfaceLayer::Create();
  surface_layer_->SetMasksToBounds(true);
  surface_layer_->SetSurfaceHitTestable(true);
  surface_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  surface_layer_->SetSurfaceId(surface_id, deadline);
  surface_layer_->SetOldestAcceptableFallback(fallback_surface_id_);

  // TODO(lfg): Investigate if it's possible to propagate the information
  // about the child surface's opacity. https://crbug.com/629851.
  bool prevent_contents_opaque_changes = true;
  child_frame_compositor_->SetLayer(surface_layer_,
                                    prevent_contents_opaque_changes,
                                    true /* is_surface_layer */);

  UpdateVisibility(true);

  surface_layer_->SetBounds(frame_size_in_dip);
}

void ChildFrameCompositingHelper::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip) {
  fallback_surface_id_ = surface_id;

  if (!surface_layer_) {
    SetSurfaceId(surface_id, frame_size_in_dip,
                 cc::DeadlinePolicy::UseDefaultDeadline());
    return;
  }

  surface_layer_->SetOldestAcceptableFallback(surface_id);
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  cc::Layer* layer = child_frame_compositor_->GetLayer();
  if (layer)
    layer->SetIsDrawable(visible);
}

}  // namespace content
