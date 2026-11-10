// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

BASE_FEATURE(kLcpEntropyGatedOnCors, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

void PopulateFrameTraceData(TracedValue& value, const LocalFrame& frame) {
  value.SetBoolean("isMainFrame", frame.IsMainFrame());
  value.SetBoolean("isOutermostMainFrame", frame.IsOutermostMainFrame());
  value.SetBoolean("isEmbeddedFrame", !frame.LocalFrameRoot().IsMainFrame() ||
                                          frame.IsInFencedFrameTree());
}

std::unique_ptr<TracedValue> CreateMetricsCandidateTraceData(
    const PaintTimingRecord& record) {
  auto value = std::make_unique<TracedValue>();
  record.PopulateTraceValue(*value);
  return value;
}

LargestContentfulPaintType GetLargestContentfulPaintTypeFromString(
    const AtomicString& type_string) {
  if (type_string.empty()) {
    return LargestContentfulPaintType::kNone;
  }

  using LargestContentfulPaintTypeMap =
      HashMap<AtomicString, LargestContentfulPaintType>;

  DEFINE_STATIC_LOCAL(LargestContentfulPaintTypeMap,
                      largest_contentful_paint_type_map,
                      ({{"svg", LargestContentfulPaintType::kSVG},
                        {"gif", LargestContentfulPaintType::kGIF},
                        {"png", LargestContentfulPaintType::kPNG},
                        {"jpg", LargestContentfulPaintType::kJPG},
                        {"avif", LargestContentfulPaintType::kAVIF},
                        {"webp", LargestContentfulPaintType::kWebP}}));

  auto it = largest_contentful_paint_type_map.find(type_string);
  if (it != largest_contentful_paint_type_map.end()) {
    return it->value;
  }

  return LargestContentfulPaintType::kNone;
}

LargestContentfulPaintType ComputeImageLargestContentfulPaintType(
    const MediaTiming& timing,
    const LocalDOMWindow* window) {
  LargestContentfulPaintType type = blink::LargestContentfulPaintType::kImage;

  // TODO(yoav): Once we'd enable the kLCPAnimatedImagesReporting flag by
  // default, we'd be able to use the value of the first animated frame time
  // directly.
  if (!timing.GetFirstVideoFrameTime().is_null()) {
    // Set the video flag.
    type |= blink::LargestContentfulPaintType::kVideo;
  } else if (timing.IsPaintedFirstFrame()) {
    // Set the animated image flag.
    type |= blink::LargestContentfulPaintType::kAnimatedImage;
  }

  // Set specific type of the image.
  type |= GetLargestContentfulPaintTypeFromString(timing.MediaType());

  // Set DataURI type.
  if (timing.IsDataUrl()) {
    type |= blink::LargestContentfulPaintType::kDataURI;
  }

  // Set cross-origin flag of the image.
  if (window) {
    auto image_url = timing.Url();
    if (!image_url.IsEmpty() && image_url.ProtocolIsInHttpFamily() &&
        window->GetFrame()->IsOutermostMainFrame()) {
      auto image_origin = SecurityOrigin::Create(image_url);
      if (!image_origin->IsSameOriginWith(window->GetSecurityOrigin())) {
        type |= blink::LargestContentfulPaintType::kCrossOrigin;
      }
    }
  }

  return type;
}

}  // namespace

struct LargestContentfulPaintCalculator::PendingMetricsData
    : public GarbageCollected<PendingMetricsData> {
  LargestImagePaintDetails image_details;
  Member<HeapTimeTicks> image_presentation_time_override;
  std::unique_ptr<TracedValue> image_trace_data;

  uint64_t text_paint_size = 0;
  std::unique_ptr<TracedValue> text_trace_data;

  void Trace(Visitor* visitor) const {
    visitor->Trace(image_presentation_time_override);
  }
};

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance,
    Delegate* delegate)
    : window_performance_(window_performance), delegate_(delegate) {
  CHECK(delegate_);
}

LargestContentfulPaintCalculator::PendingCandidateData
LargestContentfulPaintCalculator::MaybeUpdateLargestContentfulPaint(
    LcpCandidates* candidates) {
  // Compute the current web-exposed candidate size before these values get
  // updated in the call below.
  const uint64_t old_candidate_size = WebExposedCandidateSize();

  // First, figure out if there's a new candidate for metrics or performance
  // timeline.
  auto* image_candidate = candidates->Candidate<ImageRecord>();
  if (image_candidate &&
      image_candidate->EffectiveVisualSize() <= largest_painted_image_size_) {
    image_candidate = nullptr;
  }
  auto* text_candidate = candidates->Candidate<TextRecord>();
  if (text_candidate &&
      text_candidate->EffectiveVisualSize() <= largest_painted_text_size_) {
    text_candidate = nullptr;
  }
  // Since the web-exposed value is the larger of the image and text metrics
  // candidates, there cannot be a new web-exposed value if metrics didn't
  // change.
  if (!image_candidate && !text_candidate) {
    return {};
  }

  // Next, compute the new image and text candidates for metrics, along with
  // pending trace data, to be committed at presentation time.
  auto* pending_metrics = MakeGarbageCollected<PendingMetricsData>();

  if (image_candidate) {
    largest_painted_image_size_ = image_candidate->EffectiveVisualSize();
    pending_metrics->image_presentation_time_override =
        image_candidate->MetricsTimestampOverride();
    pending_metrics->image_details =
        CreateLargestImagePaintDetails(*image_candidate);
    if (delegate_->IsHardNavigation() && PaintTimingDetector::IsTracing()) {
      pending_metrics->image_trace_data =
          CreateMetricsCandidateTraceData(*image_candidate);
    }
  }

  if (text_candidate) {
    largest_painted_text_size_ = text_candidate->EffectiveVisualSize();
    pending_metrics->text_paint_size = text_candidate->EffectiveVisualSize();
    if (delegate_->IsHardNavigation() && PaintTimingDetector::IsTracing()) {
      pending_metrics->text_trace_data =
          CreateMetricsCandidateTraceData(*text_candidate);
    }
  }

  // Next, set up the metrics callback to flush the candidates.
  PendingCandidateData candidate_data;
  candidate_data.metrics_callback = blink::BindOnce(
      &LargestContentfulPaintCalculator::
          FlushPendingMetricsDataOnFramePresented,
      WrapWeakPersistent(this), WrapPersistent(pending_metrics));

  const uint64_t new_candidate_size = WebExposedCandidateSize();
  CHECK_GE(new_candidate_size, old_candidate_size);

  // Finally, compute the pending LCP entry and corresponding trace data, if
  // there's a new web-exposed value.
  if (new_candidate_size > old_candidate_size) {
    if (largest_painted_image_size_ >= largest_painted_text_size_) {
      CHECK(image_candidate);
      candidate_data.lcp_entry =
          CreateLargestContentfulPaintEntry(*image_candidate);
      candidate_data.lcp_trace_data =
          CreateWebExposedLcpCandidateTraceData(*image_candidate);
    } else {
      CHECK(text_candidate);
      candidate_data.lcp_entry =
          CreateLargestContentfulPaintEntry(*text_candidate);
      candidate_data.lcp_trace_data =
          CreateWebExposedLcpCandidateTraceData(*text_candidate);
    }
  }

  return candidate_data;
}

void LargestContentfulPaintCalculator::FlushPendingMetricsDataOnFramePresented(
    PendingMetricsData* pending_metrics,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  // Commit the pending candidates, updating internal state in preparation for
  // flushing.
  CommitPendingMetricsImageCandidate(pending_metrics, presentation_timestamp);
  CommitPendingMetricsTextCandidate(pending_metrics, presentation_timestamp);

  // Flush candidates, taking into account `largest_pending_image_`.
  MaybeFlushMetricsCandidates();
}

void LargestContentfulPaintCalculator::CommitPendingMetricsImageCandidate(
    PendingMetricsData* pending_metrics,
    const base::TimeTicks& presentation_timestamp) {
  if (pending_metrics->image_details.paint_size == 0) {
    return;
  }

  // Set presentation time, either from the override (first video and animated
  // image frame) or `presentation_timestamp` (otherwise).
  //
  // TODO(crbug.com/449779010): This behavior should be unified between hard
  // and soft navigations.
  HeapTimeTicks* timestamp = pending_metrics->image_presentation_time_override;
  pending_metrics->image_details.presentation_time =
      delegate_->IsHardNavigation() && timestamp && !timestamp->value.is_null()
          ? timestamp->value
          : presentation_timestamp;

  largest_presented_image_trace_data_ =
      std::move(pending_metrics->image_trace_data);
  largest_presented_image_details_ = pending_metrics->image_details;

  // Clear `largest_pending_image_` if we've caught up.
  if (largest_pending_image_ &&
      largest_presented_image_details_.paint_size >=
          largest_pending_image_->EffectiveVisualSize()) {
    largest_pending_image_ = nullptr;
  }
}

void LargestContentfulPaintCalculator::CommitPendingMetricsTextCandidate(
    PendingMetricsData* pending_metrics,
    const base::TimeTicks& presentation_timestamp) {
  if (pending_metrics->text_paint_size == 0) {
    return;
  }
  largest_presented_text_details_.presentation_time = presentation_timestamp;
  largest_presented_text_details_.paint_size = pending_metrics->text_paint_size;
  largest_presented_text_trace_data_ =
      std::move(pending_metrics->text_trace_data);
}

void LargestContentfulPaintCalculator::MaybeFlushMetricsCandidates() {
  bool image_update = UpdateMetricsIfLargestPendingOrPresentedImageChanged();
  bool text_update = UpdateMetricsIfLargestPresentedTextChanged();
  if (image_update || text_update) {
    delegate_->OnLcpMetricsForReportingChanged();
  }
}

LargestImagePaintDetails
LargestContentfulPaintCalculator::CreateLargestImagePaintDetails(
    const ImageRecord& image_record) const {
  LargestImagePaintDetails image_details;
  if (const MediaTiming* timing = image_record.GetMediaTiming()) {
    image_details.type = ComputeImageLargestContentfulPaintType(
        *timing, window_performance_->DomWindow());
    image_details.resource_load_timings.discovery_time =
        timing->DiscoveryTime();
    image_details.resource_load_timings.load_start = timing->LoadStart();
    image_details.resource_load_timings.load_end = timing->LoadEnd();
  }
  image_details.request_priority = image_record.RequestPriority();
  image_details.paint_size = image_record.EffectiveVisualSize();
  image_details.bpp = image_record.EntropyForLCP();
  return image_details;
}

LargestContentfulPaint*
LargestContentfulPaintCalculator::CreateLargestContentfulPaintEntry(
    const ImageRecord& largest_image) {
  const MediaTiming* media_timing = largest_image.GetMediaTiming();
  Node* image_node = largest_image.GetNode();

  CHECK(media_timing);
  CHECK(image_node);

  const KURL& url = media_timing->Url();
  const String& image_string = url.GetString();
  const String& image_url =
      url.ProtocolIsData()
          ? image_string.substr(0, ElementTiming::kInlineImageMaxChars)
          : image_string;
  // Do not expose element attribution from shadow trees.
  Element* image_element =
      image_node->IsInShadowTree() ? nullptr : To<Element>(image_node);
  const AtomicString& image_id =
      image_element ? image_element->GetIdAttribute() : AtomicString();

  CHECK(window_performance_);
  return MakeGarbageCollected<LargestContentfulPaint>(
      largest_image.EffectiveVisualSize(),
      window_performance_->MonotonicTimeToDOMHighResTimeStamp(
          largest_image.LoadTime()),
      image_id, image_url, image_element, window_performance_->DomWindow());
}

LargestContentfulPaint*
LargestContentfulPaintCalculator::CreateLargestContentfulPaintEntry(
    const TextRecord& largest_text) {
  CHECK(window_performance_);
  Node* text_node = largest_text.GetNode();
  CHECK(text_node);

  // Do not expose element attribution from shadow trees. Also note that @page
  // margin boxes do not create Element nodes.
  Element* text_element =
      text_node->IsInShadowTree() ? nullptr : DynamicTo<Element>(text_node);
  const AtomicString& text_id =
      text_element ? text_element->GetIdAttribute() : AtomicString();

  // Always use paint time as start time for text LCP candidate.
  return MakeGarbageCollected<LargestContentfulPaint>(
      /*size=*/largest_text.EffectiveVisualSize(),
      /*load_time=*/
      window_performance_->MonotonicTimeToDOMHighResTimeStamp(
          base::TimeTicks()),
      /*id=*/text_id,
      /*url=*/g_empty_string,
      /*element=*/text_element, window_performance_->DomWindow());
}

bool LargestContentfulPaintCalculator::
    UpdateMetricsIfLargestPendingOrPresentedImageChanged() {
  // `largest_pending_image_` is cleared when it is no longer larger than
  // largest presented image, so if it's non-null it should be used.
  if (largest_pending_image_) {
    CHECK_GT(largest_pending_image_->EffectiveVisualSize(),
             largest_presented_image_details_.paint_size);

    // If the size is the same, there's nothing to update. Note that the
    // `largest_pending_image_` can change to be something smaller if the
    // previous largest pending image's node was removed.
    if (largest_pending_image_->EffectiveVisualSize() ==
        latest_lcp_details_.largest_image.paint_size) {
      return false;
    }

    // For animated images, it's possible there's a metrics override time for
    // first frame, but wait until sufficiently loaded to set the presentation
    // time and handle tracing, etc.
    latest_lcp_details_.largest_image =
        CreateLargestImagePaintDetails(*largest_pending_image_);
    return true;
  }

  // Otherwise, use `largest_presented_image_details_` if that changed.
  if (largest_presented_image_details_.IsEquivalentTo(
          latest_lcp_details_.largest_image)) {
    return false;
  }

  latest_lcp_details_.largest_image = largest_presented_image_details_;

  // If the `largest_pending_image_` is removed before another image is
  // presented, this will fall back empty state.
  if (latest_lcp_details_.largest_image.paint_size == 0) {
    CHECK(latest_lcp_details_.largest_image.presentation_time.is_null());
    return true;
  }

  CHECK(!latest_lcp_details_.largest_image.presentation_time.is_null());

  std::unique_ptr<TracedValue> trace_data =
      std::move(largest_presented_image_trace_data_);
  if (trace_data && PaintTimingDetector::IsTracing()) {
    ReportMetricsCandidateToTrace(
        std::move(trace_data), "LargestImagePaint::Candidate",
        latest_lcp_details_.largest_image.presentation_time,
        ++ukm_largest_image_candidate_count_);
  }

  return true;
}

bool LargestContentfulPaintCalculator::
    UpdateMetricsIfLargestPresentedTextChanged() {
  if (largest_presented_text_details_.IsEquivalentTo(
          latest_lcp_details_.largest_text)) {
    return false;
  }

  CHECK(!largest_presented_text_details_.presentation_time.is_null());
  CHECK_GT(largest_presented_text_details_.paint_size, 0u);

  latest_lcp_details_.largest_text = largest_presented_text_details_;

  std::unique_ptr<TracedValue> trace_data =
      std::move(largest_presented_text_trace_data_);
  if (trace_data && PaintTimingDetector::IsTracing()) {
    ReportMetricsCandidateToTrace(
        std::move(trace_data), "LargestTextPaint::Candidate",
        largest_presented_text_details_.presentation_time,
        ++ukm_largest_text_candidate_count_);
  }
  return true;
}

void LargestContentfulPaintCalculator::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
  visitor->Trace(delegate_);
  visitor->Trace(largest_pending_image_);
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::CreateWebExposedLcpCandidateTraceData(
    const PaintTimingRecord& record) {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window || !TRACE_EVENT_CATEGORY_ENABLED(
                     paint_timing::kWebExposedLcpTraceCategories)) {
    return nullptr;
  }

  auto value = std::make_unique<TracedValue>();
  value->SetInteger("nodeId", record.NodeIdForTracing());
  value->SetInteger("size", static_cast<int>(record.EffectiveVisualSize()));
  value->SetInteger("candidateIndex", ++web_exposed_candidate_count_);
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  if (delegate_->IsHardNavigation()) {
    value->SetString("navigationId", IdentifiersFactory::LoaderId(
                                         window->document()->Loader()));
  }
  value->SetInteger(
      "performanceTimelineNavigationId",
      static_cast<int>(window_performance_->NavigationId().web_exposed_id));
  value->SetString("nodeName", record.GetNode()->DebugName());
  if (const auto* image_record = DynamicTo<ImageRecord>(record)) {
    value->SetString("type", "image");
    const MediaTiming* media_timing = image_record->GetMediaTiming();
    CHECK(media_timing);
    value->SetDouble("imageDiscoveryTime",
                     window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                         media_timing->DiscoveryTime()));
    value->SetDouble("imageLoadStart",
                     window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                         media_timing->LoadStart()));
    value->SetDouble("imageLoadEnd",
                     window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                         media_timing->LoadEnd()));
    if (auto* html_image_element =
            DynamicTo<HTMLImageElement>(image_record->GetNode())) {
      const AtomicString& loading_attr =
          html_image_element->FastGetAttribute(html_names::kLoadingAttr);
      value->SetString("loadingAttr", loading_attr);
    }
  } else {
    CHECK(record.IsTextRecord());
    value->SetString("type", "text");
  }
  return value;
}

void LargestContentfulPaintCalculator::ReportMetricsCandidateToTrace(
    std::unique_ptr<TracedValue> value,
    const char* trace_event_name,
    base::TimeTicks presentation_time,
    unsigned candidate_index) {
  CHECK(!presentation_time.is_null());
  value->SetInteger("candidateIndex", candidate_index);
  LocalFrame* frame = window_performance_->DomWindow()->GetFrame();
  CHECK(frame);
  PopulateFrameTraceData(*value, *frame);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", trace_event_name,
                                   presentation_time, "data", std::move(value),
                                   "frame", GetFrameIdForTracing(frame));
}

void LargestContentfulPaintCalculator::OnImageFirstPaint(ImageRecord* record) {
  if (!IsEligibleForLcp(*record)) {
    return;
  }
  if (record->EffectiveVisualSize() >
          largest_presented_image_details_.paint_size &&
      record->IsEffectiveSizeLargerThan(largest_pending_image_)) {
    largest_pending_image_ = record;
  }
}

void LargestContentfulPaintCalculator::OnImageRemoved(
    const LayoutObject& object,
    const MediaTiming* timing) {
  // TODO(crbug.com/457794552): This causes metrics to fall back to the
  // `largest_presented_image_details_`, but there are a couple problems with
  // this:
  //  - What if there's a larger pending image and the page unloads? We might
  //    want to iterate through the list of pending image records to get the
  //    next largest pending image.
  //  - Metrics won't be updated until something else triggers calling
  //    `MaybeFlushMetricsCandidates()`, e.g. a new largest text or image paint.
  //    We should probably update metrics sooner and not rely on this.
  if (!largest_pending_image_) {
    return;
  }
  if (largest_pending_image_->GetMediaTiming() == timing &&
      largest_pending_image_->ImageGeneratingLayoutObject() == &object) {
    largest_pending_image_ = nullptr;
  }
}

LargestContentfulPaintCalculator::LcpCandidates::LcpCandidates() = default;

void LargestContentfulPaintCalculator::LcpCandidates::MaybeUpdateCandidate(
    TextRecord* record) {
  if (record->IsEffectiveSizeLargerThan(text_candidate_)) {
    text_candidate_ = record;
  }
}

void LargestContentfulPaintCalculator::LcpCandidates::MaybeUpdateCandidate(
    ImageRecord* record) {
  if (record->IsEffectiveSizeLargerThan(image_candidate_)) {
    image_candidate_ = record;
  }
}

void LargestContentfulPaintCalculator::LcpCandidates::Trace(
    Visitor* visitor) const {
  visitor->Trace(image_candidate_);
  visitor->Trace(text_candidate_);
}

bool LargestContentfulPaintCalculator::IsEligibleForLcp(
    const ImageRecord& record) const {
  // Unlike text records, image records are only created if there is non-zero
  // size.
  CHECK_GT(record.EffectiveVisualSize(), 0u);

  if (record.GetEffectiveVisualSizeResult().is_viewport_covered) {
    return false;
  }

  // First video frame records are queued outside of paint, so the associated
  // `Node` or `MediaTiming` can be removed and GCed before the next paint.
  // Ignore these records.
  //
  // TODO(crbug.com/562498378): We should keep a strong reference to the
  // `VideoTiming` rather than ignoring these records since the behavior depends
  // on garbage collection.
  if (!record.GetMediaTiming() || record.WasNodeRemoved()) {
    return false;
  }

  // Only apply the minimum entropy check to CORS-same-origin resources to
  // prevent cross-origin size leaks (crbug.com/502288792).
  if (base::FeatureList::IsEnabled(kLcpEntropyGatedOnCors) &&
      !record.IsCorsSameOrigin()) {
    return true;
  }

  // The first video frame often fails to meet the entropy check, e.g. due to
  // being a solid color or blank frame. This is problematic for ICP, so we
  // ignore the entropy check in that case.
  if (!record.GetMediaTiming()->GetFirstVideoFrameTime().is_null() &&
      (!delegate_->IsHardNavigation() ||
       RuntimeEnabledFeatures::EntropyIgnoredForFirstVideoFrameLCPEnabled())) {
    return true;
  }

  return record.GetEffectiveVisualSizeResult().is_min_entropy_met;
}

bool LargestContentfulPaintCalculator::IsEligibleForLcp(
    const TextRecord& record) const {
  return record.EffectiveVisualSize() > 0;
}

// static
EffectiveVisualSizeResult
LargestContentfulPaintCalculator::ComputeEffectiveVisualSize(
    const LayoutObject& object,
    const MediaTiming& media_timing,
    const gfx::Rect& image_border,
    const gfx::RectF& mapped_visual_rect,
    const gfx::Size& intrinsic_size,
    uint64_t viewport_area,
    const PaintTimingDetector& detector) {
  EffectiveVisualSizeResult result;

  // "1. Let width be intersectionRect’s width, rounded up to the nearest
  //     integer."
  // "2. Let height be intersectionRect’s height, rounded up to the nearest
  //    integer."
  // "3. Let size be width * height."
  uint64_t size = mapped_visual_rect.size().GetArea();

  // If the rect occupies the whole viewport, disregard this candidate by saying
  // the size is 0.
  //
  // "4. Let root be document’s browsing context’s top-level browsing context’s
  //     active document."
  // "5. Let rootWidth be root’s visual viewport’s width, excluding any
  //     scrollbars."
  // "6. Let rootHeight be root’s visual viewport’s height, excluding any
  //     scrollbars."
  // "7. If size is equal to rootWidth times rootHeight, return null."
  //
  // Note: An SVG image size is computed with respect to the virtual viewport of
  // the SVG, so `size` can be larger than `viewport_area` in edge cases.
  if (size >= viewport_area) {
    // Rather than returning null, return the actual size to differentiate from
    // the 0-size case, which downstream processing handles differently.
    result.size = size;
    result.is_viewport_covered = true;
    return result;
  }

  // "8. If imageRequest is not null, run the following steps to adjust for
  //     image position and upscaling:"
  //
  // ...
  //
  // "8.9  Let naturalArea be imageRequest’s natural width * imageRequest’s
  //       natural height."
  const uint64_t natural_area = intrinsic_size.Area64();

  // "8.10 If naturalArea is 0, then return null."
  if (natural_area == 0) {
    return result;
  }

  // "8.11 Let boundingClientArea be clientContentRect’s width *
  //       clientContentRect’s height."
  //
  // Transform visual rect to window before downscaling.
  gfx::RectF bounding_client_rect =
      detector.BlinkSpaceToDIPs(gfx::RectF(image_border));
  const uint64_t bounding_client_area = bounding_client_rect.size().GetArea();

  // "8.12 Let scaleFactor be boundingClientArea / naturalArea."
  // "8.13 If scaleFactor is greater than 1, then divide size by scaleFactor."
  if (bounding_client_area > natural_area) {
    size = static_cast<double>(size) * natural_area / bounding_client_area;
  }

  // "8.1 If imageRequest’s response’s content length in bytes is less than
  //      size * 0.004, then return null.
  //
  // TODO(crbug.com/534835237): This constant differs from the spec, which uses
  // 0.032 in the equivalent formulation.
  constexpr double kMinimumEntropyForLCP = 0.05;
  result.entropy = media_timing.ContentSizeForEntropy() * 8.0 / size;
  result.is_min_entropy_met = result.entropy >= kMinimumEntropyForLCP;

  result.size = size;
  return result;
}

}  // namespace blink
