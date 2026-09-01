// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

namespace {

// In the context of FCP++, we define contentful background image as one that
// satisfies all of the following conditions:
// * has image reources attached to style of the object, i.e.,
//  { background-image: url('example.gif') }
// * not attached to <body> or <html>
// This function contains the above heuristics.
bool IsBackgroundImageContentful(const LayoutObject& object,
                                 const Image& image) {
  // Background images attached to <body> or <html> are likely for background
  // purpose, so we rule them out.
  if (IsA<LayoutView>(object) || object.IsBody() ||
      object.IsDocumentElement()) {
    return false;
  }
  return true;
}

void ReportImagePixelInaccuracy(HTMLImageElement* image_element) {
  DCHECK(image_element);
  ImageResourceContent* image_content = image_element->CachedImage();
  if (!image_content || !image_content->IsLoaded()) {
    return;
  }
  Document& document = image_element->GetDocument();
  // Get the intrinsic dimensions from the image resource
  gfx::Size intrinsic_dimensions =
      image_content->IntrinsicSize(kRespectImageOrientation);

  // Get the layout dimensions and screen DPR
  uint32_t layout_width = image_element->LayoutBoxWidth();
  uint32_t layout_height = image_element->LayoutBoxHeight();
  float document_dpr = document.DevicePixelRatio();

  // Get the size attribute calculated width, if any
  std::optional<float> sizes_width = image_element->GetResourceWidth();
  // Report offset in pixels between intrinsic and layout dimensions
  const float kDPRCap = 2.0;
  float capped_dpr = std::min(document_dpr, kDPRCap);
  uint64_t fetched_pixels = intrinsic_dimensions.Area64();
  uint64_t needed_pixels = base::saturated_cast<uint64_t>(
      (layout_width * document_dpr) * (layout_height * document_dpr));
  uint64_t capped_pixels = base::saturated_cast<uint64_t>(
      (layout_width * capped_dpr) * (layout_height * capped_dpr));

  bool has_overfetched_pixels = fetched_pixels > needed_pixels;
  base::UmaHistogramBoolean("Renderer.Images.HasOverfetchedPixels",
                            has_overfetched_pixels);
  if (has_overfetched_pixels) {
    uint64_t overfetched_pixels = fetched_pixels - needed_pixels;
    base::UmaHistogramCounts10M("Renderer.Images.OverfetchedPixels",
                                base::saturated_cast<int>(overfetched_pixels));
  }

  bool has_overfetched_capped_pixels = fetched_pixels > capped_pixels;
  base::UmaHistogramBoolean("Renderer.Images.HasOverfetchedCappedPixels",
                            has_overfetched_capped_pixels);
  if (has_overfetched_capped_pixels) {
    uint64_t overfetched_capped_pixels = fetched_pixels - capped_pixels;
    base::UmaHistogramCounts10M(
        "Renderer.Images.OverfetchedCappedPixels",
        base::saturated_cast<int>(overfetched_capped_pixels));
  }

  // Report offset in pixels between layout width and sizes result
  if (sizes_width) {
    int sizes_miss =
        base::saturated_cast<int>(sizes_width.value() - layout_width);

    base::UmaHistogramBoolean("Renderer.Images.HasSizesAttributeMiss",
                              sizes_miss > 0);
    if (sizes_miss > 0) {
      base::UmaHistogramCounts10000("Renderer.Images.SizesAttributeMiss",
                                    sizes_miss);
    }
  }
}

}  // namespace

PaintTimingDetector::PaintTimingDetector(PaintTiming* paint_timing)
    : paint_timing_(paint_timing),
      text_paint_timing_detector_(
          MakeGarbageCollected<TextPaintTimingDetector>(this)),
      image_paint_timing_detector_(
          MakeGarbageCollected<ImagePaintTimingDetector>(this)) {
  if (PaintTimingVisualizer::IsTracingEnabled()) {
    visualizer_ = std::make_unique<PaintTimingVisualizer>();
  }
}

void PaintTimingDetector::NotifyPaintFinished() {
  if (PaintTimingVisualizer::IsTracingEnabled()) {
    if (!visualizer_) {
      visualizer_ = std::make_unique<PaintTimingVisualizer>();
    }
    visualizer_->RecordMainFrameViewport(*this, GetFrame());
  } else {
    visualizer_.reset();
  }
}

// static
PaintTimingDetector& PaintTimingDetector::From(Document& document) {
  return PaintTiming::From(document).GetPaintTimingDetector();
}

// static
bool PaintTimingDetector::NotifyBackgroundImagePaint(
    Node& node,
    const Image& image,
    const StyleImage& style_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  LayoutObject* object = node.GetLayoutObject();
  if (!object) {
    return false;
  }

  auto& paint_timing = PaintTiming::From(object->GetDocument());
  paint_timing.GetImageElementTiming()->NotifyBackgroundImagePaint(
      node, style_image, current_paint_chunk_properties, image_border);

  if (!IsBackgroundImageContentful(*object, image)) {
    return false;
  }

  ImageResourceContent* cached_image = style_image.CachedImage();
  DCHECK(cached_image);
  // TODO(yoav): |image| and |cached_image.GetImage()| are not the same here in
  // the case of SVGs. Figure out why and if we can remove this footgun.
  return paint_timing.GetPaintTimingDetector()
      .GetImagePaintTimingDetector()
      .RecordImage(*object, image.Size(), *cached_image,
                   current_paint_chunk_properties, &style_image, image_border);
}

// static
bool PaintTimingDetector::NotifyImagePaint(
    const LayoutObject& object,
    const gfx::Size& intrinsic_size,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  auto& paint_timing = PaintTiming::From(object.GetDocument());
  paint_timing.GetImageElementTiming()->NotifyImagePaint(
      object, media_timing, current_paint_chunk_properties, image_border);

  if (IgnorePaintTimingScope::ShouldIgnore()) {
    return false;
  }

  Node* image_node = object.GetNode();
  HTMLImageElement* element = DynamicTo<HTMLImageElement>(image_node);

  if (element) {
    // This doesn't capture poster. That's probably fine.
    ReportImagePixelInaccuracy(element);
  }

  return paint_timing.GetPaintTimingDetector()
      .GetImagePaintTimingDetector()
      .RecordImage(object, intrinsic_size, media_timing,
                   current_paint_chunk_properties, nullptr, image_border);
}

// static
void PaintTimingDetector::NotifyFirstVideoFrame(
    const LayoutObject& object,
    const gfx::Size& intrinsic_size,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  if (NotifyImagePaint(object, intrinsic_size, media_timing,
                       current_paint_chunk_properties, image_border)) {
    // crbug.com/434659231: Paint timing callbacks happen as part of paint, and
    // since the first video frame notification happens outside of paint, this
    // `media_timing` will not be considered until the next frame. Request a
    // frame now to prevent delays in timing.
    object.GetFrameView()->ScheduleAnimation();
  }
}

// static
void PaintTimingDetector::NotifyInteractionTriggeredVideoSrcChange(
    const LayoutObject& object) {
  PaintTimingDetector::From(object.GetDocument())
      .GetImagePaintTimingDetector()
      .NotifyInteractionTriggeredVideoSrcChange(object);
}

void PaintTimingDetector::NotifyImageFinished(const LayoutObject& object,
                                              const MediaTiming* media_timing) {
  image_paint_timing_detector_->NotifyImageFinished(object, media_timing);
}

void PaintTimingDetector::NotifyBackgroundImageFinished(
    const StyleFetchedImage* image) {
  image_paint_timing_detector_->NotifyBackgroundImageFinished(image);
}

void PaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  paint_timing_->GetImageElementTiming()->NotifyImageRemoved(object,
                                                             cached_image);
  image_paint_timing_detector_->NotifyImageRemoved(object, cached_image);
}

void PaintTimingDetector::DidChangePerformanceTiming() {
  paint_timing::NotifyLoaderPerformanceTimingChanged(
      paint_timing_->GetDocument());
}

gfx::RectF PaintTimingDetector::BlinkSpaceToDIPs(const gfx::RectF& rect) const {
  FrameWidget* widget = GetFrame().GetWidgetForLocalRoot();
  // May be nullptr in tests.
  if (!widget) {
    return rect;
  }
  return widget->BlinkSpaceToDIPs(rect);
}

gfx::RectF PaintTimingDetector::CalculateVisualRect(
    const gfx::Rect& visual_rect,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties) const {
  // This case should be dealt with outside the function.
  DCHECK(!visual_rect.IsEmpty());

  // As Layout objects live in different transform spaces, the object's rect
  // should be projected to the viewport's transform space.
  FloatClipRect float_clip_visual_rect((gfx::RectF(visual_rect)));
  const LocalFrame& local_root = GetFrame().LocalFrameRoot();
  GeometryMapper::LocalToAncestorVisualRect(current_paint_chunk_properties,
                                            local_root.ContentLayoutObject()
                                                ->FirstFragment()
                                                .LocalBorderBoxProperties(),
                                            float_clip_visual_rect);
  if (local_root.IsOutermostMainFrame()) {
    return BlinkSpaceToDIPs(float_clip_visual_rect.Rect());
  }

  // TODO(crbug.com/1346602): Enabling frames from a fenced frame tree to map
  // to the outermost main frame enables fenced content to learn about its
  // position in the embedder which can be used to communicate from embedder to
  // embeddee. For now, return the rect in the local root (not great for remote
  // frames) to avoid introducing a side channel but this will require design
  // work to fix in the long term.
  if (local_root.IsInFencedFrameTree()) {
    return BlinkSpaceToDIPs(float_clip_visual_rect.Rect());
  }

  // OOPIF. The final rect lives in the iframe's root frame space. We need to
  // project it to the top frame space.
  auto layout_visual_rect =
      PhysicalRect::EnclosingRect(float_clip_visual_rect.Rect());
  GetFrame().LocalFrameRoot().View()->MapToVisualRectInRemoteRootFrame(
      layout_visual_rect);
  return BlinkSpaceToDIPs(gfx::RectF(layout_visual_rect));
}

void PaintTimingDetector::ReportIgnoredContent() {
  text_paint_timing_detector_->ReportLargestIgnoredText();
  image_paint_timing_detector_->ReportLargestIgnoredImage();
}

ScopedPaintTimingDetectorBlockPaintHook*
    ScopedPaintTimingDetectorBlockPaintHook::top_ = nullptr;

void ScopedPaintTimingDetectorBlockPaintHook::EmplaceIfNeeded(
    const LayoutBoxModelObject& aggregator,
    const PropertyTreeStateOrAlias& property_tree_state) {
  if (IgnorePaintTimingScope::IgnoreDepth() > 1) {
    return;
  }
  // |reset_top_| is unset when |aggregator| is anonymous so that each
  // aggregation corresponds to an element. See crbug.com/988593. When set,
  // |top_| becomes |this|, and |top_| is restored to the previous value when
  // the ScopedPaintTimingDetectorBlockPaintHook goes out of scope.
  if (!aggregator.GetNode()) {
    return;
  }

  reset_top_.emplace(&top_, this);
  TextPaintTimingDetector& detector =
      PaintTimingDetector::From(aggregator.GetDocument())
          .GetTextPaintTimingDetector();
  // Only set |data_| if we need to walk the object.
  if (detector.ShouldWalkObject(aggregator)) {
    data_.emplace(aggregator, property_tree_state, &detector);
  }
}

ScopedPaintTimingDetectorBlockPaintHook::Data::Data(
    const LayoutBoxModelObject& aggregator,
    const PropertyTreeStateOrAlias& property_tree_state,
    TextPaintTimingDetector* detector)
    : aggregator_(aggregator),
      property_tree_state_(property_tree_state),
      detector_(detector) {}

ScopedPaintTimingDetectorBlockPaintHook::
    ~ScopedPaintTimingDetectorBlockPaintHook() {
  if (!data_ || data_->aggregated_visual_rect_.IsEmpty()) {
    return;
  }
  // TODO(crbug.com/987804): Checking |ShouldWalkObject| again is necessary
  // because the result can change, but more investigation is needed as to why
  // the change is possible.
  if (!data_->detector_ ||
      !data_->detector_->ShouldWalkObject(data_->aggregator_)) {
    return;
  }
  data_->detector_->RecordAggregatedText(data_->aggregator_,
                                         data_->aggregated_visual_rect_,
                                         data_->property_tree_state_);
}

void PaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(text_paint_timing_detector_);
  visitor->Trace(image_paint_timing_detector_);
  visitor->Trace(paint_timing_);
}

LocalDOMWindow* PaintTimingDetector::DomWindow() const {
  return paint_timing_->GetDocument()->domWindow();
}

LocalFrame& PaintTimingDetector::GetFrame() const {
  return CHECK_DEREF(paint_timing_->GetDocument()->GetFrame());
}

}  // namespace blink
