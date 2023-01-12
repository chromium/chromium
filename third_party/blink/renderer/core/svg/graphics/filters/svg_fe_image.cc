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
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
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

static AffineTransform MakeMapBetweenRects(const gfx::RectF& source,
                                           const gfx::RectF& dest) {
  AffineTransform transform;
  transform.Translate(dest.x() - source.x(), dest.y() - source.y());
  transform.Scale(dest.width() / source.width(),
                  dest.height() / source.height());
  return transform;
}

static absl::optional<AffineTransform> ComputeViewportAdjustmentTransform(
    const SVGElement* element,
    const gfx::RectF& target_rect) {
  // If we're referencing an element with percentage units, eg. <rect
  // with="30%"> those values were resolved against the viewport.  Build up a
  // transformation that maps from the viewport space to the filter primitive
  // subregion.
  // TODO(crbug/260709): This fixes relative lengths but breaks non-relative
  // ones.
  gfx::SizeF viewport_size = SVGLengthContext(element).ResolveViewport();
  if (viewport_size.IsEmpty()) {
    return absl::nullopt;
  }
  return MakeMapBetweenRects(gfx::RectF(viewport_size), target_rect);
}

gfx::RectF FEImage::MapInputs(const gfx::RectF&) const {
  gfx::RectF dest_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
  if (const LayoutObject* layout_object = ReferencedLayoutObject()) {
    gfx::RectF src_rect = GetLayoutObjectRepaintRect(*layout_object);
    if (element_->HasRelativeLengths()) {
      auto viewport_transform =
          ComputeViewportAdjustmentTransform(element_, dest_rect);
      if (viewport_transform)
        src_rect = viewport_transform->MapRect(src_rect);
    } else {
      src_rect = GetFilter()->MapLocalRectToAbsoluteRect(src_rect);
      src_rect.Offset(dest_rect.x(), dest_rect.y());
    }
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

static gfx::RectF IntersectWithFilterRegion(const Filter* filter,
                                            const gfx::RectF& rect) {
  gfx::RectF filter_region = filter->FilterRegion();
  // Workaround for crbug.com/512453.
  if (filter_region.IsEmpty())
    return rect;
  return IntersectRects(rect,
                        filter->MapLocalRectToAbsoluteRect(filter_region));
}

sk_sp<PaintFilter> FEImage::CreateImageFilterForLayoutObject(
    const LayoutObject& layout_object) {
  gfx::RectF dst_rect =
      GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
  gfx::RectF src_rect = GetLayoutObjectRepaintRect(layout_object);

  AffineTransform transform;
  if (element_->HasRelativeLengths()) {
    auto viewport_transform =
        ComputeViewportAdjustmentTransform(element_, dst_rect);
    if (viewport_transform) {
      src_rect = viewport_transform->MapRect(src_rect);
      transform = *viewport_transform;
    }
  } else {
    src_rect = GetFilter()->MapLocalRectToAbsoluteRect(src_rect);
    src_rect.Offset(dst_rect.x(), dst_rect.y());
    transform.Translate(dst_rect.x(), dst_rect.y());
  }
  // Intersect with the (transformed) source rect to remove "empty" bits of the
  // image.
  dst_rect.Intersect(src_rect);

  // Clip the filter primitive rect by the filter region and use that as the
  // cull rect for the paint record.
  gfx::RectF crop_rect = IntersectWithFilterRegion(GetFilter(), dst_rect);
  PaintRecorder paint_recorder;
  cc::PaintCanvas* canvas = paint_recorder.beginRecording();
  canvas->concat(AffineTransformToSkM44(transform));
  {
    auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
    SVGObjectPainter(layout_object).PaintResourceSubtree(builder->Context());
    builder->EndRecording(*canvas);
  }
  return sk_make_sp<RecordPaintFilter>(
      paint_recorder.finishRecordingAsPicture(), gfx::RectFToSkRect(crop_rect));
}

sk_sp<PaintFilter> FEImage::CreateImageFilter() {
  // The current implementation assumes this primitive is always set to clip to
  // the filter bounds.
  DCHECK(ClipsToBounds());
  if (const auto* layout_object = ReferencedLayoutObject())
    return CreateImageFilterForLayoutObject(*layout_object);

  if (PaintImage image =
          image_ ? image_->PaintImageForCurrentFrame() : PaintImage()) {
    gfx::RectF src_rect(gfx::SizeF(image_->Size()));
    gfx::RectF dst_rect =
        GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
    preserve_aspect_ratio_->TransformRect(dst_rect, src_rect);
    // Clip the filter primitive rect by the filter region and adjust the source
    // rectangle if needed.
    gfx::RectF crop_rect = IntersectWithFilterRegion(GetFilter(), dst_rect);
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
