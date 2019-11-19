// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_painter.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

// TODO(loonybear): Currently oversized-images policy is only reinforced on
// HTMLImageElement. Use data from |layout_image|, |content_rect| and/or
// Document to support this policy on other image types (crbug.com/930281).
bool CheckForOversizedImagesPolicy(const LayoutImage& layout_image,
                                   scoped_refptr<Image> image) {
  DCHECK(image);
  if (!RuntimeEnabledFeatures::UnoptimizedImagePoliciesEnabled(
          &layout_image.GetDocument()))
    return false;

  DoubleSize layout_size(layout_image.ContentSize());
  IntSize image_size = image->Size();
  if (layout_size.IsEmpty() || image_size.IsEmpty())
    return false;

  double dpr = layout_image.GetDocument().GetFrame()->DevicePixelRatio();
  double downscale_ratio_width =
      image_size.Width() / (dpr * layout_size.Width());
  double downscale_ratio_height =
      image_size.Height() / (dpr * layout_size.Height());

  const LayoutImageResource* image_resource = layout_image.ImageResource();
  const ImageResourceContent* cached_image =
      image_resource ? image_resource->CachedImage() : nullptr;
  const String& image_url =
      cached_image ? cached_image->Url().GetString() : g_empty_string;

  return !layout_image.GetDocument().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kOversizedImages,
      blink::PolicyValue(
          std::max(downscale_ratio_width, downscale_ratio_height),
          blink::mojom::PolicyValueType::kDecDouble),
      ReportOptions::kReportOnFailure, g_empty_string, image_url);
}

}  // namespace

void ImagePainter::Paint(const PaintInfo& paint_info) {
  layout_image_.LayoutReplaced::Paint(paint_info);

  if (paint_info.phase == PaintPhase::kOutline)
    PaintAreaElementFocusRing(paint_info);
}

void ImagePainter::PaintAreaElementFocusRing(const PaintInfo& paint_info) {
  Document& document = layout_image_.GetDocument();

  if (paint_info.IsPrinting() ||
      !document.GetFrame()->Selection().FrameIsFocusedAndActive())
    return;

  auto* area_element = DynamicTo<HTMLAreaElement>(document.FocusedElement());
  if (!area_element)
    return;

  if (area_element->ImageElement() != layout_image_.GetNode())
    return;

  // Even if the theme handles focus ring drawing for entire elements, it won't
  // do it for an area within an image, so we don't call
  // LayoutTheme::themeDrawsFocusRing here.

  // We use EnsureComputedStyle() instead of GetComputedStyle() here because
  // <area> is used and its style applied even if it has display:none.
  const ComputedStyle* area_element_style = area_element->EnsureComputedStyle();
  // If the outline width is 0 we want to avoid drawing anything even if we
  // don't use the value directly.
  if (!area_element_style->OutlineWidth())
    return;

  Path path = area_element->GetPath(&layout_image_);
  if (path.IsEmpty())
    return;

  ScopedPaintState paint_state(layout_image_, paint_info);
  auto paint_offset = paint_state.PaintOffset();
  path.Translate(FloatSize(paint_offset));

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_image_, DisplayItem::kImageAreaFocusRing))
    return;

  DrawingRecorder recorder(paint_info.context, layout_image_,
                           DisplayItem::kImageAreaFocusRing);

  // FIXME: Clip path instead of context when Skia pathops is ready.
  // https://crbug.com/251206

  paint_info.context.Save();
  PhysicalRect focus_rect = layout_image_.PhysicalContentBoxRect();
  focus_rect.Move(paint_offset);
  paint_info.context.Clip(PixelSnappedIntRect(focus_rect));
  paint_info.context.DrawFocusRing(
      path, area_element_style->GetOutlineStrokeWidthForFocusRing(),
      area_element_style->OutlineOffset(),
      layout_image_.ResolveColor(*area_element_style,
                                 GetCSSPropertyOutlineColor()));
  paint_info.context.Restore();
}

void ImagePainter::PaintReplaced(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  LayoutSize content_size = layout_image_.ContentSize();
  bool has_image = layout_image_.ImageResource()->HasImage();

  if (has_image) {
    if (content_size.IsEmpty())
      return;
  } else {
    if (paint_info.phase == PaintPhase::kSelection)
      return;
    if (content_size.Width() <= 2 || content_size.Height() <= 2)
      return;
  }

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, layout_image_,
                                                  paint_info.phase))
    return;

  // Disable cache in under-invalidation checking mode for animated image
  // because it may change before it's actually invalidated.
  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      layout_image_.ImageResource() &&
      layout_image_.ImageResource()->MaybeAnimated())
    cache_skipper.emplace(context);

  PhysicalRect content_rect(
      paint_offset + layout_image_.PhysicalContentBoxOffset(),
      PhysicalSizeToBeNoop(content_size));

  if (!has_image) {
    // Draw an outline rect where the image should be.
    IntRect paint_rect = PixelSnappedIntRect(content_rect);
    DrawingRecorder recorder(context, layout_image_, paint_info.phase);
    context.SetStrokeStyle(kSolidStroke);
    context.SetStrokeColor(Color::kLightGray);
    context.SetFillColor(Color::kTransparent);
    context.DrawRect(paint_rect);
    return;
  }

  PhysicalRect paint_rect = layout_image_.ReplacedContentRect();
  paint_rect.offset += paint_offset;

  DrawingRecorder recorder(context, layout_image_, paint_info.phase);
  DCHECK(paint_info.PaintContainer());
  PaintIntoRect(context, paint_rect, content_rect);
}

void ImagePainter::PaintIntoRect(GraphicsContext& context,
                                 const PhysicalRect& dest_rect,
                                 const PhysicalRect& content_rect) {
  if (!layout_image_.ImageResource()->HasImage() ||
      layout_image_.ImageResource()->ErrorOccurred())
    return;  // FIXME: should we just ASSERT these conditions? (audit all
             // callers).

  IntRect pixel_snapped_dest_rect = PixelSnappedIntRect(dest_rect);
  if (pixel_snapped_dest_rect.IsEmpty())
    return;

  scoped_refptr<Image> image =
      layout_image_.ImageResource()->GetImage(pixel_snapped_dest_rect.Size());
  if (!image || image->IsNull())
    return;

  FloatRect src_rect = FloatRect(image->Rect());
  // If the content rect requires clipping, adjust |srcRect| and
  // |pixelSnappedDestRect| over using a clip.
  if (!content_rect.Contains(dest_rect)) {
    IntRect pixel_snapped_content_rect = PixelSnappedIntRect(content_rect);
    pixel_snapped_content_rect.Intersect(pixel_snapped_dest_rect);
    if (pixel_snapped_content_rect.IsEmpty())
      return;
    src_rect = MapRect(FloatRect(pixel_snapped_content_rect),
                       FloatRect(pixel_snapped_dest_rect), src_rect);
    pixel_snapped_dest_rect = pixel_snapped_content_rect;
  }

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               inspector_paint_image_event::Data(layout_image_, src_rect,
                                                 FloatRect(dest_rect)));

  ScopedInterpolationQuality interpolation_quality_scope(
      context, layout_image_.StyleRef().GetInterpolationQuality());

  Node* node = layout_image_.GetNode();
  Image::ImageDecodingMode decode_mode =
      IsHTMLImageElement(node)
          ? ToHTMLImageElement(node)->GetDecodingModeForPainting(
                image->paint_image_id())
          : Image::kUnspecifiedDecode;

  // TODO(loonybear): Support image policies on other image types in addition to
  // HTMLImageElement.
  if (auto* image_element = ToHTMLImageElementOrNull(node)) {
    if (CheckForOversizedImagesPolicy(layout_image_, image) ||
        image_element->IsImagePolicyViolated()) {
      // Does not set an observer for the placeholder image, setting it to null.
      scoped_refptr<PlaceholderImage> placeholder_image =
          PlaceholderImage::Create(nullptr, image->Size(),
                                   image->Data() ? image->Data()->size() : 0);
      placeholder_image->SetIconAndTextScaleFactor(
          layout_image_.GetFrame()->PageZoomFactor());
      image = std::move(placeholder_image);
    }
  }

  context.DrawImage(
      image.get(), decode_mode, FloatRect(pixel_snapped_dest_rect), &src_rect,
      layout_image_.StyleRef().HasFilterInducingProperty(),
      SkBlendMode::kSrcOver,
      LayoutObject::ShouldRespectImageOrientation(&layout_image_));
  if ((IsHTMLImageElement(node) || IsHTMLVideoElement(node)) &&
      !context.ContextDisabled() && layout_image_.CachedImage() &&
      layout_image_.CachedImage()->IsLoaded()) {
    LocalDOMWindow* window = layout_image_.GetDocument().domWindow();
    DCHECK(window);
    ImageElementTiming::From(*window).NotifyImagePainted(
        &layout_image_, layout_image_.CachedImage(),
        context.GetPaintController().CurrentPaintChunkProperties());
  }
  PaintTimingDetector::NotifyImagePaint(
      layout_image_, image->Size(), layout_image_.CachedImage(),
      context.GetPaintController().CurrentPaintChunkProperties());
}

}  // namespace blink
