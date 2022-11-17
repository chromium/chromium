// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_image_painter.h"

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"

namespace blink {

namespace {
ImagePaintTimingInfo ComputeImagePaintTimingInfo(
    const LayoutSVGImage& layout_image,
    const Image& image,
    const ImageResourceContent* image_content,
    const GraphicsContext& context,
    const gfx::Rect& image_border) {
  return ImagePaintTimingInfo(PaintTimingDetector::NotifyImagePaint(
      layout_image, image.Size(), *image_content,
      context.GetPaintController().CurrentPaintChunkProperties(),
      image_border));
}
}  // namespace

void SVGImagePainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_svg_image_.StyleRef().Visibility() != EVisibility::kVisible ||
      !layout_svg_image_.ImageResource()->HasImage())
    return;

  if (SVGModelObjectPainter::CanUseCullRect(layout_svg_image_.StyleRef())) {
    if (!paint_info.GetCullRect().IntersectsTransformed(
            layout_svg_image_.LocalSVGTransform(),
            layout_svg_image_.VisualRectInLocalSVGCoordinates()))
      return;
  }
  // Images cannot have children so do not call TransformCullRect.

  ScopedSVGTransformState transform_state(paint_info, layout_svg_image_);
  {
    ScopedSVGPaintState paint_state(layout_svg_image_, paint_info);
    SVGModelObjectPainter::RecordHitTestData(layout_svg_image_, paint_info);
    SVGModelObjectPainter::RecordRegionCaptureData(layout_svg_image_,
                                                   paint_info);
    if (!DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_svg_image_, paint_info.phase)) {
      SVGDrawingRecorder recorder(paint_info.context, layout_svg_image_,
                                  paint_info.phase);
      PaintForeground(paint_info);
    }
  }

  SVGModelObjectPainter(layout_svg_image_).PaintOutline(paint_info);
}

void SVGImagePainter::PaintForeground(const PaintInfo& paint_info) {
  const LayoutImageResource& image_resource =
      *layout_svg_image_.ImageResource();
  gfx::SizeF image_viewport_size = ComputeImageViewportSize();
  image_viewport_size.Scale(layout_svg_image_.StyleRef().EffectiveZoom());
  if (image_viewport_size.IsEmpty())
    return;

  scoped_refptr<Image> image = image_resource.GetImage(image_viewport_size);
  gfx::RectF dest_rect = layout_svg_image_.ObjectBoundingBox();
  auto* image_element = To<SVGImageElement>(layout_svg_image_.GetElement());
  RespectImageOrientationEnum respect_orientation =
      image_resource.ImageOrientation();

  gfx::RectF src_rect(image->SizeAsFloat(respect_orientation));
  if (respect_orientation && !image->HasDefaultOrientation()) {
    // We need the oriented source rect for adjusting the aspect ratio
    gfx::SizeF unadjusted_size = src_rect.size();
    image_element->preserveAspectRatio()->CurrentValue()->TransformRect(
        dest_rect, src_rect);

    // Map the oriented_src_rect back into the src_rect space
    src_rect =
        image->CorrectSrcRectForImageOrientation(unadjusted_size, src_rect);
  } else {
    image_element->preserveAspectRatio()->CurrentValue()->TransformRect(
        dest_rect, src_rect);
  }

  ImageResourceContent* image_content = image_resource.CachedImage();
  if (image_content->IsLoaded()) {
    LocalDOMWindow* window = layout_svg_image_.GetDocument().domWindow();
    DCHECK(window);
    ImageElementTiming::From(*window).NotifyImagePainted(
        layout_svg_image_, *image_content,
        paint_info.context.GetPaintController().CurrentPaintChunkProperties(),
        gfx::ToEnclosingRect(dest_rect));
  }
  PaintTiming& timing = PaintTiming::From(layout_svg_image_.GetDocument());
  timing.MarkFirstContentfulPaint();

  ScopedInterpolationQuality interpolation_quality_scope(
      paint_info.context,
      layout_svg_image_.StyleRef().GetInterpolationQuality());
  Image::ImageDecodingMode decode_mode =
      image_element->GetDecodingModeForPainting(image->paint_image_id());
  auto image_auto_dark_mode = ImageClassifierHelper::GetImageAutoDarkMode(
      *layout_svg_image_.GetFrame(), layout_svg_image_.StyleRef(), dest_rect,
      src_rect);
  paint_info.context.DrawImage(
      *image, decode_mode, image_auto_dark_mode,
      ComputeImagePaintTimingInfo(layout_svg_image_, *image, image_content,
                                  paint_info.context,
                                  gfx::ToEnclosingRect(dest_rect)),
      dest_rect, &src_rect, SkBlendMode::kSrcOver, respect_orientation);
}

gfx::SizeF SVGImagePainter::ComputeImageViewportSize() const {
  DCHECK(layout_svg_image_.ImageResource()->HasImage());

  if (To<SVGImageElement>(layout_svg_image_.GetElement())
          ->preserveAspectRatio()
          ->CurrentValue()
          ->Align() != SVGPreserveAspectRatio::kSvgPreserveaspectratioNone)
    return layout_svg_image_.ObjectBoundingBox().size();

  ImageResourceContent* cached_image =
      layout_svg_image_.ImageResource()->CachedImage();

  // Images with preserveAspectRatio=none should force non-uniform scaling. This
  // can be achieved by setting the image's container size to its viewport size
  // (i.e. concrete object size returned by the default sizing algorithm.)  See
  // https://www.w3.org/TR/SVG/single-page.html#coords-PreserveAspectRatioAttribute
  // and https://drafts.csswg.org/css-images-3/#default-sizing.

  // Avoid returning the size of the broken image.
  if (cached_image->ErrorOccurred())
    return gfx::SizeF();
  Image* image = cached_image->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    return svg_image->ConcreteObjectSize(
        layout_svg_image_.ObjectBoundingBox().size());
  }
  // The orientation here does not matter. Just use kRespectImageOrientation.
  return image->SizeAsFloat(kRespectImageOrientation);
}

}  // namespace blink
