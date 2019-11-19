// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_image_painter.h"

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void SVGImagePainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_svg_image_.StyleRef().Visibility() != EVisibility::kVisible ||
      !layout_svg_image_.ImageResource()->HasImage())
    return;

  PaintInfo paint_info_before_filtering(paint_info);

  if (SVGModelObjectPainter(layout_svg_image_)
          .CullRectSkipsPainting(paint_info_before_filtering)) {
    return;
  }
  // Images cannot have children so do not call TransformCullRect.

  ScopedSVGTransformState transform_state(
      paint_info_before_filtering, layout_svg_image_,
      layout_svg_image_.LocalToSVGParentTransform());
  {
    ScopedSVGPaintState paint_state(layout_svg_image_,
                                    paint_info_before_filtering);
    if (paint_state.ApplyClipMaskAndFilterIfNecessary() &&
        !DrawingRecorder::UseCachedDrawingIfPossible(
            paint_state.GetPaintInfo().context, layout_svg_image_,
            paint_state.GetPaintInfo().phase)) {
      SVGModelObjectPainter::RecordHitTestData(layout_svg_image_, paint_info);
      DrawingRecorder recorder(paint_state.GetPaintInfo().context,
                               layout_svg_image_,
                               paint_state.GetPaintInfo().phase);
      PaintForeground(paint_state.GetPaintInfo());
    }
  }

  SVGModelObjectPainter(layout_svg_image_)
      .PaintOutline(paint_info_before_filtering);
}

void SVGImagePainter::PaintForeground(const PaintInfo& paint_info) {
  const LayoutImageResource* image_resource = layout_svg_image_.ImageResource();
  FloatSize image_viewport_size = ComputeImageViewportSize();
  image_viewport_size.Scale(layout_svg_image_.StyleRef().EffectiveZoom());
  if (image_viewport_size.IsEmpty())
    return;

  scoped_refptr<Image> image =
      image_resource->GetImage(ExpandedIntSize(image_viewport_size));
  FloatRect dest_rect = layout_svg_image_.ObjectBoundingBox();
  FloatRect src_rect(0, 0, image->width(), image->height());

  SVGImageElement* image_element =
      ToSVGImageElement(layout_svg_image_.GetElement());
  image_element->preserveAspectRatio()->CurrentValue()->TransformRect(dest_rect,
                                                                      src_rect);
  ScopedInterpolationQuality interpolation_quality_scope(
      paint_info.context,
      layout_svg_image_.StyleRef().GetInterpolationQuality());
  Image::ImageDecodingMode decode_mode =
      image_element->GetDecodingModeForPainting(image->paint_image_id());
  paint_info.context.DrawImage(
      image.get(), decode_mode, dest_rect, &src_rect,
      layout_svg_image_.StyleRef().HasFilterInducingProperty());
  if (!paint_info.context.ContextDisabled() && image_resource->CachedImage() &&
      image_resource->CachedImage()->IsLoaded()) {
    LocalDOMWindow* window = layout_svg_image_.GetDocument().domWindow();
    DCHECK(window);
    DCHECK(paint_info.PaintContainer());
    ImageElementTiming::From(*window).NotifyImagePainted(
        &layout_svg_image_, image_resource->CachedImage(),
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  PaintTimingDetector::NotifyImagePaint(
      layout_svg_image_, image->Size(), image_resource->CachedImage(),
      paint_info.context.GetPaintController().CurrentPaintChunkProperties());
}

FloatSize SVGImagePainter::ComputeImageViewportSize() const {
  DCHECK(layout_svg_image_.ImageResource()->HasImage());

  if (ToSVGImageElement(layout_svg_image_.GetElement())
          ->preserveAspectRatio()
          ->CurrentValue()
          ->Align() != SVGPreserveAspectRatio::kSvgPreserveaspectratioNone)
    return layout_svg_image_.ObjectBoundingBox().Size();

  ImageResourceContent* cached_image =
      layout_svg_image_.ImageResource()->CachedImage();

  // Images with preserveAspectRatio=none should force non-uniform scaling. This
  // can be achieved by setting the image's container size to its viewport size
  // (i.e. concrete object size returned by the default sizing algorithm.)  See
  // https://www.w3.org/TR/SVG/single-page.html#coords-PreserveAspectRatioAttribute
  // and https://drafts.csswg.org/css-images-3/#default-sizing.

  // Avoid returning the size of the broken image.
  if (cached_image->ErrorOccurred())
    return FloatSize();
  Image* image = cached_image->GetImage();
  if (image->IsSVGImage()) {
    return ToSVGImage(image)->ConcreteObjectSize(
        layout_svg_image_.ObjectBoundingBox().Size());
  }
  return FloatSize(image->Size());
}

}  // namespace blink
