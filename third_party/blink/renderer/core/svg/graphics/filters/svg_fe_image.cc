/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_fe_image.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

FEImage::FEImage(Filter* filter,
                 scoped_refptr<Image> image,
                 const SVGPreserveAspectRatio* preserve_aspect_ratio)
    : FilterEffect(filter),
      image_(std::move(image)),
      preserve_aspect_ratio_(preserve_aspect_ratio) {
  FilterEffect::SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

FEImage::FEImage(Filter* filter,
                 const SVGElement* element,
                 const SVGPreserveAspectRatio* preserve_aspect_ratio)
    : FilterEffect(filter),
      element_(element),
      preserve_aspect_ratio_(preserve_aspect_ratio) {
  FilterEffect::SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
}

void FEImage::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(preserve_aspect_ratio_);
  FilterEffect::Trace(visitor);
}

static gfx::RectF GetLayoutObjectRepaintRect(
    const LayoutObject& layout_object) {
  return layout_object.LocalToSVGParentTransform().MapRect(
      layout_object.VisualRectInLocalSVGCoordinates());
}

static gfx::SizeF ComputeViewportAdjustmentScale(
    const LayoutObject& layout_object,
    const gfx::SizeF& target_size) {
  // If we're referencing an element with percentage units, eg. <rect
  // with="30%"> those values were resolved against the viewport.  Build up a
  // transformation that maps from the viewport space to the filter primitive
  // subregion.
  // TODO(crbug/260709): This fixes relative lengths but breaks non-relative
  // ones.
  const gfx::SizeF viewport_size =
      SVGViewportResolver(layout_object).ResolveViewport();
  if (viewport_size.IsEmpty()) {
    return gfx::SizeF(1, 1);
  }
  return gfx::SizeF(target_size.width() / viewport_size.width(),
                    target_size.height() / viewport_size.height());
}

AffineTransform FEImage::SourceToDestinationTransform(
    const LayoutObject& layout_object,
    const gfx::RectF& dest_rect) const {
  gfx::SizeF viewport_scale(GetFilter()->Scale(), GetFilter()->Scale());
  if (element_->HasRelativeLengths()) {
    viewport_scale =
        ComputeViewportAdjustmentScale(layout_object, dest_rect.size());
  }
  AffineTransform transform;
  transform.Translate(dest_rect.x(), dest_rect.y());
  transform.Scale(viewport_scale.width(), viewport_scale.height());
  return transform;
}

gfx::RectF FEImage::MapInputs(const gfx::RectF&) const {
  gfx::RectF dest_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
  if (const LayoutObject* layout_object = ReferencedLayoutObject()) {
    const AffineTransform transform =
        SourceToDestinationTransform(*layout_object, dest_rect);
    const gfx::RectF src_rect =
        transform.MapRect(GetLayoutObjectRepaintRect(*layout_object));
    dest_rect.Intersect(src_rect);
    return dest_rect;
  }
  if (image_) {
    gfx::RectF src_rect(gfx::SizeF(image_->Size()));
    preserve_aspect_ratio_->TransformRect(dest_rect, src_rect);
    return dest_rect;
  }
  return gfx::RectF();
}

const LayoutObject* FEImage::ReferencedLayoutObject() const {
  if (!element_)
    return nullptr;
  return element_->GetLayoutObject();
}

WTF::TextStream& FEImage::ExternalRepresentation(WTF::TextStream& ts,
                                                 int indent) const {
  gfx::Size image_size;
  if (image_) {
    image_size = image_->Size();
  } else if (const LayoutObject* layout_object = ReferencedLayoutObject()) {
    image_size =
        gfx::ToEnclosingRect(GetLayoutObjectRepaintRect(*layout_object)).size();
  }
  WriteIndent(ts, indent);
  ts << "[feImage";
  FilterEffect::ExternalRepresentation(ts);
  ts << " image-size=\"" << image_size.width() << "x" << image_size.height()
     << "\"]\n";
  // FIXME: should this dump also object returned by SVGFEImage::image() ?
  return ts;
}

sk_sp<PaintFilter> FEImage::CreateImageFilterForLayoutObject(
    const LayoutObject& layout_object,
    const gfx::RectF& dst_rect,
    const gfx::RectF& crop_rect) {
  const AffineTransform transform =
      SourceToDestinationTransform(layout_object, dst_rect);
  const gfx::RectF src_rect =
      transform.MapRect(GetLayoutObjectRepaintRect(layout_object));
  // Intersect with the (transformed) source rect to remove "empty" bits of the
  // image.
  const gfx::RectF cull_rect = gfx::IntersectRects(crop_rect, src_rect);

  PaintRecorder paint_recorder;
  cc::PaintCanvas* canvas = paint_recorder.beginRecording();
  canvas->concat(AffineTransformToSkM44(transform));
  {
    PaintRecordBuilder builder;
    SVGObjectPainter(layout_object, nullptr)
        .PaintResourceSubtree(builder.Context());
    builder.EndRecording(*canvas);
  }
  return sk_make_sp<RecordPaintFilter>(
      paint_recorder.finishRecordingAsPicture(), gfx::RectFToSkRect(cull_rect));
}

sk_sp<PaintFilter> FEImage::CreateImageFilter() {
  // The current implementation assumes this primitive is always set to clip to
  // the filter bounds.
  DCHECK(ClipsToBounds());
  gfx::RectF crop_rect =
      gfx::SkRectToRectF(GetCropRect().value_or(PaintFilter::CropRect()));
  gfx::RectF dst_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
  if (const auto* layout_object = ReferencedLayoutObject()) {
    return CreateImageFilterForLayoutObject(*layout_object, dst_rect,
                                            crop_rect);
  }
  if (PaintImage image =
          image_ ? image_->PaintImageForCurrentFrame() : PaintImage()) {
    gfx::RectF src_rect(gfx::SizeF(image_->Size()));
    preserve_aspect_ratio_->TransformRect(dst_rect, src_rect);
    // Adjust the source rectangle if the primitive has been cropped.
    if (crop_rect != dst_rect)
      src_rect = gfx::MapRect(crop_rect, dst_rect, src_rect);
    return sk_make_sp<ImagePaintFilter>(
        std::move(image), gfx::RectFToSkRect(src_rect),
        gfx::RectFToSkRect(crop_rect), cc::PaintFlags::FilterQuality::kHigh);
  }
  // "A href reference that is an empty image (zero width or zero height),
  //  that fails to download, is non-existent, or that cannot be displayed
  //  (e.g. because it is not in a supported image format) fills the filter
  //  primitive subregion with transparent black."
  return CreateTransparentBlack();
}

}  // namespace blink
