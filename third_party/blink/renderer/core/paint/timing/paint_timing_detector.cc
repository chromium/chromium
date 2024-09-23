// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
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
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
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

PaintTimingDetector::PaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      text_paint_timing_detector_(
          MakeGarbageCollected<TextPaintTimingDetector>(frame_view,
                                                        this,
                                                        nullptr /*set later*/)),
      image_paint_timing_detector_(
          MakeGarbageCollected<ImagePaintTimingDetector>(
              frame_view,
              nullptr /*set later*/)),
      callback_manager_(
          MakeGarbageCollected<PaintTimingCallbackManagerImpl>(frame_view)) {
  if (PaintTimingVisualizer::IsTracingEnabled()) {
    visualizer_.emplace();
  }
  text_paint_timing_detector_->ResetCallbackManager(callback_manager_.Get());
  image_paint_timing_detector_->ResetCallbackManager(callback_manager_.Get());
}

void PaintTimingDetector::NotifyPaintFinished() {
  if (PaintTimingVisualizer::IsTracingEnabled()) {
    if (!visualizer_) {
      visualizer_.emplace();
    }
    visualizer_->RecordMainFrameViewport(*frame_view_);
  } else {
    visualizer_.reset();
  }
  text_paint_timing_detector_->OnPaintFinished();
  if (image_paint_timing_detector_) {
    image_paint_timing_detector_->OnPaintFinished();
  }
  if (callback_manager_->CountCallbacks() > 0) {
    callback_manager_->RegisterPaintTimeCallbackForCombinedCallbacks();
  }
  LocalDOMWindow* window = frame_view_->GetFrame().DomWindow();
  if (window) {
    DOMWindowPerformance::performance(*window)->OnPaintFinished();
  }
  if (Document* document = frame_view_->GetFrame().GetDocument()) {
    document->OnPaintFinished();
  }
}

// static
bool PaintTimingDetector::NotifyBackgroundImagePaint(
    const Node& node,
    const Image& image,
    const StyleImage& style_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  LayoutObject* object = node.GetLayoutObject();
  if (!object) {
    return false;
  }
  LocalFrameView* frame_view = object->GetFrameView();
  if (!frame_view) {
    return false;
  }

  PaintTimingDetector& paint_timing_detector =
      frame_view->GetPaintTimingDetector();
  if (paint_timing_detector.IsUnrelatedSoftNavigationPaint(node)) {
    return false;
  }
  ImagePaintTimingDetector& image_paint_timing_detector =
      paint_timing_detector.GetImagePaintTimingDetector();
  if (!image_paint_timing_detector.IsRecordingLargestImagePaint()) {
    return false;
  }

  if (!IsBackgroundImageContentful(*object, image)) {
    return false;
  }

  ImageResourceContent* cached_image = style_image.CachedImage();
  DCHECK(cached_image);
  // TODO(yoav): |image| and |cached_image.GetImage()| are not the same here in
  // the case of SVGs. Figure out why and if we can remove this footgun.

  return image_paint_timing_detector.RecordImage(
      *object, image.Size(), *cached_image, current_paint_chunk_properties,
      &style_image, image_border);
}

// static
bool PaintTimingDetector::NotifyImagePaint(
    const LayoutObject& object,
    const gfx::Size& intrinsic_size,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  if (IgnorePaintTimingScope::ShouldIgnore()) {
    return false;
  }
  LocalFrameView* frame_view = object.GetFrameView();
  if (!frame_view) {
    return false;
  }
  PaintTimingDetector& paint_timing_detector =
      frame_view->GetPaintTimingDetector();
  ImagePaintTimingDetector& image_paint_timing_detector =
      paint_timing_detector.GetImagePaintTimingDetector();
  if (!image_paint_timing_detector.IsRecordingLargestImagePaint()) {
    return false;
  }

  Node* image_node = object.GetNode();
  if (image_node &&
      paint_timing_detector.IsUnrelatedSoftNavigationPaint(*image_node)) {
    return false;
  }
  HTMLImageElement* element = DynamicTo<HTMLImageElement>(image_node);

  if (element) {
    // This doesn't capture poster. That's probably fine.
    ReportImagePixelInaccuracy(element);
  }

  return image_paint_timing_detector.RecordImage(
      object, intrinsic_size, media_timing, current_paint_chunk_properties,
      nullptr, image_border);
}

void PaintTimingDetector::NotifyImageFinished(const LayoutObject& object,
                                              const MediaTiming* media_timing) {
  if (IgnorePaintTimingScope::ShouldIgnore() ||
      !image_paint_timing_detector_->IsRecordingLargestImagePaint()) {
    return;
  }
  image_paint_timing_detector_->NotifyImageFinished(object, media_timing);
}

void PaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  text_paint_timing_detector_->LayoutObjectWillBeDestroyed(object);
}

void PaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  if (image_paint_timing_detector_->IsRecordingLargestImagePaint()) {
    image_paint_timing_detector_->NotifyImageRemoved(object, cached_image);
  }
}

void PaintTimingDetector::OnInputOrScroll() {
  // If we have already stopped and we're no longer recording the largest image
  // paint, then abort.
  if (!image_paint_timing_detector_->IsRecordingLargestImagePaint()) {
    return;
  }

  // TextPaintTimingDetector is used for both Largest Contentful Paint and for
  // Element Timing. Therefore, here we only want to stop recording Largest
  // Contentful Paint.
  text_paint_timing_detector_->StopRecordingLargestTextPaint();
  // ImagePaintTimingDetector is currently only being used for
  // LargestContentfulPaint.
  image_paint_timing_detector_->StopRecordEntries();
  image_paint_timing_detector_->StopRecordingLargestImagePaint();
  largest_contentful_paint_calculator_ = nullptr;
  record_lcp_to_metrics_ = false;

  // Set first_input_or_scroll_notified_timestamp_ only once.
  if (first_input_or_scroll_notified_timestamp_ == base::TimeTicks()) {
    first_input_or_scroll_notified_timestamp_ = base::TimeTicks::Now();
  }

  DidChangePerformanceTiming();
}

void PaintTimingDetector::NotifyInputEvent(WebInputEvent::Type type) {
  // A single keyup event should be ignored. It could be caused by user actions
  // such as refreshing via Ctrl+R.
  if (type == WebInputEvent::Type::kMouseMove ||
      type == WebInputEvent::Type::kMouseEnter ||
      type == WebInputEvent::Type::kMouseLeave ||
      type == WebInputEvent::Type::kKeyUp ||
      WebInputEvent::IsPinchGestureEventType(type)) {
    return;
  }
  OnInputOrScroll();
}

void PaintTimingDetector::NotifyScroll(mojom::blink::ScrollType scroll_type) {
  if (scroll_type != mojom::blink::ScrollType::kUser &&
      scroll_type != mojom::blink::ScrollType::kCompositor) {
    return;
  }
  OnInputOrScroll();
}

bool PaintTimingDetector::NeedToNotifyInputOrScroll() const {
  DCHECK(text_paint_timing_detector_);
  return text_paint_timing_detector_->IsRecordingLargestTextPaint() ||
         image_paint_timing_detector_;
}

void PaintTimingDetector::RestartRecordingLCP() {
  text_paint_timing_detector_->RestartRecordingLargestTextPaint();
  image_paint_timing_detector_->RestartRecordingLargestImagePaint();
  lcp_was_restarted_ = true;
  soft_navigation_was_detected_ = false;
  GetLargestContentfulPaintCalculator()->ResetMetricsLcp();
}

void PaintTimingDetector::SoftNavigationDetected(LocalDOMWindow* window) {
  soft_navigation_was_detected_ = true;
  auto* lcp_calculator = GetLargestContentfulPaintCalculator();
  // If the window is detached (no calculator) or we haven't yet got any
  // presentation times for neither a text record nor an image one, bail. The
  // web exposed entry will get updated when the presentation times callback
  // will be called.
  if (!lcp_calculator || (!potential_soft_navigation_text_record_ &&
                          !potential_soft_navigation_image_record_)) {
    return;
  }
  if (!lcp_was_restarted_ ||
      RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
    lcp_calculator->UpdateWebExposedLargestContentfulPaintIfNeeded(
        potential_soft_navigation_text_record_,
        potential_soft_navigation_image_record_,
        /*is_triggered_by_soft_navigation=*/lcp_was_restarted_);
  }

  // Report the soft navigation LCP to metrics.
  CHECK(record_soft_navigation_lcp_for_metrics_);
  soft_navigation_lcp_details_for_metrics_ =
      largest_contentful_paint_calculator_->LatestLcpDetails();
  DidChangePerformanceTiming();
}

void PaintTimingDetector::RestartRecordingLCPToUkm() {
  text_paint_timing_detector_->RestartRecordingLargestTextPaint();
  image_paint_timing_detector_->RestartRecordingLargestImagePaint();
  record_soft_navigation_lcp_for_metrics_ = true;
  // Reset the lcp candidate and the soft navigation LCP for reporting to UKM
  // when a new soft navigation happens. When this resetting happens, the
  // previous lcp details should already be updated.
  soft_navigation_lcp_details_for_metrics_ = LargestContentfulPaintDetails();
}

LargestContentfulPaintCalculator*
PaintTimingDetector::GetLargestContentfulPaintCalculator() {
  if (largest_contentful_paint_calculator_) {
    return largest_contentful_paint_calculator_.Get();
  }

  auto* dom_window = frame_view_->GetFrame().DomWindow();
  if (!dom_window) {
    return nullptr;
  }

  largest_contentful_paint_calculator_ =
      MakeGarbageCollected<LargestContentfulPaintCalculator>(
          DOMWindowPerformance::performance(*dom_window));
  return largest_contentful_paint_calculator_.Get();
}

void PaintTimingDetector::UpdateMetricsLcp() {
  // The DidChangePerformanceTiming method which triggers the reporting of
  // metrics LCP would not be called when we are not recording metrics LCP.
  if (!record_lcp_to_metrics_ && !record_soft_navigation_lcp_for_metrics_) {
    return;
  }

  if (record_lcp_to_metrics_) {
    auto latest_lcp_details =
        GetLargestContentfulPaintCalculator()->LatestLcpDetails();
    lcp_details_for_metrics_ = latest_lcp_details;
  }

  // If we're waiting on a softnav and it wasn't detected yet, keep on waiting
  // and don't update.
  if (record_soft_navigation_lcp_for_metrics_ &&
      soft_navigation_was_detected_) {
    auto latest_lcp_details =
        GetLargestContentfulPaintCalculator()->LatestLcpDetails();
    soft_navigation_lcp_details_for_metrics_ = latest_lcp_details;
  }

  DidChangePerformanceTiming();
}

void PaintTimingDetector::DidChangePerformanceTiming() {
  Document* document = frame_view_->GetFrame().GetDocument();
  if (!document) {
    return;
  }
  DocumentLoader* loader = document->Loader();
  if (!loader) {
    return;
  }
  loader->DidChangePerformanceTiming();
}

gfx::RectF PaintTimingDetector::BlinkSpaceToDIPs(const gfx::RectF& rect) const {
  FrameWidget* widget = frame_view_->GetFrame().GetWidgetForLocalRoot();
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
  const LocalFrame& local_root = frame_view_->GetFrame().LocalFrameRoot();
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
  frame_view_->GetFrame()
      .LocalFrameRoot()
      .View()
      ->MapToVisualRectInRemoteRootFrame(layout_visual_rect);
  return BlinkSpaceToDIPs(gfx::RectF(layout_visual_rect));
}

void PaintTimingDetector::UpdateLcpCandidate() {
  auto* lcp_calculator = GetLargestContentfulPaintCalculator();
  if (!lcp_calculator) {
    return;
  }

  // * nullptr means there is no new candidate update, which could be caused by
  // user input or no content show up on the page.
  // * Record.paint_time == 0 means there is an image but the image is still
  // loading. The perf API should wait until the paint-time is available.
  std::pair<TextRecord*, bool> text_update_result = {nullptr, false};
  std::pair<ImageRecord*, bool> image_update_result = {nullptr, false};

  if (text_paint_timing_detector_->IsRecordingLargestTextPaint()) {
    text_update_result = text_paint_timing_detector_->UpdateMetricsCandidate();
  }

  if (image_paint_timing_detector_->IsRecordingLargestImagePaint()) {
    image_update_result =
        image_paint_timing_detector_->UpdateMetricsCandidate();
  }

  if (image_update_result.second || text_update_result.second) {
    UpdateMetricsLcp();
  }
  // If we stopped and then restarted LCP measurement (to support soft
  // navigations), and didn't yet detect a soft navigation, put aside the
  // records as potential soft navigation LCP ones, and don't update the web
  // exposed entries just yet. We'll do that once we actually detect the soft
  // navigation.
  if (lcp_was_restarted_ && !soft_navigation_was_detected_) {
    potential_soft_navigation_text_record_ = text_update_result.first;
    potential_soft_navigation_image_record_ = image_update_result.first;
    return;
  }
  potential_soft_navigation_text_record_ = nullptr;
  potential_soft_navigation_image_record_ = nullptr;

  // If we're still recording the initial LCP, or if LCP was explicitly
  // restarted for soft navigations, fire the web exposed entry.
  if (record_lcp_to_metrics_ || lcp_was_restarted_) {
    lcp_calculator->UpdateWebExposedLargestContentfulPaintIfNeeded(
        text_update_result.first, image_update_result.first,
        /*is_triggered_by_soft_navigation=*/lcp_was_restarted_);
  }
}

void PaintTimingDetector::ReportIgnoredContent() {
  text_paint_timing_detector_->ReportLargestIgnoredText();
  if (image_paint_timing_detector_->IsRecordingLargestImagePaint()) {
    image_paint_timing_detector_->ReportLargestIgnoredImage();
  }
}

const LargestContentfulPaintDetails&
PaintTimingDetector::LatestLcpDetailsForTest() const {
  return largest_contentful_paint_calculator_->LatestLcpDetails();
}

bool PaintTimingDetector::IsUnrelatedSoftNavigationPaint(const Node& node) {
  return (WasLCPRestarted() &&
          !(IsSoftNavigationDetected() || node.IsModifiedBySoftNavigation()));
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
  TextPaintTimingDetector& detector = aggregator.GetFrameView()
                                          ->GetPaintTimingDetector()
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
  visitor->Trace(frame_view_);
  visitor->Trace(largest_contentful_paint_calculator_);
  visitor->Trace(callback_manager_);
  visitor->Trace(potential_soft_navigation_image_record_);
  visitor->Trace(potential_soft_navigation_text_record_);
}

}  // namespace blink
