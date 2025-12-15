// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"

#include "base/check.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/navigation_id_generator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr const char kTraceCategories[] = "loading,rail,devtools.timeline";

constexpr const char kLCPCandidate[] = "largestContentfulPaint::Candidate";

constexpr const char kLCPCandidateForSoftNavs[] =
    "largestContentfulPaint::CandidateForSoftNavigation";

void PopulateFrameTraceData(TracedValue& value, const LocalFrame& frame) {
  value.SetBoolean("isMainFrame", frame.IsMainFrame());
  value.SetBoolean("isOutermostMainFrame", frame.IsOutermostMainFrame());
  value.SetBoolean("isEmbeddedFrame", !frame.LocalFrameRoot().IsMainFrame() ||
                                          frame.IsInFencedFrameTree());
}

}  // namespace

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

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance,
    Delegate* delegate)
    : window_performance_(window_performance), delegate_(delegate) {
  CHECK(delegate_);
}

void LargestContentfulPaintCalculator::
    UpdateWebExposedLargestContentfulPaintIfNeeded(
        const TextRecord* largest_text,
        const ImageRecord* largest_image) {
  uint64_t text_size = largest_text ? largest_text->RecordedSize() : 0u;
  uint64_t image_size = largest_image ? largest_image->RecordedSize() : 0u;
  if (image_size > text_size) {
    if (image_size > largest_reported_size_ && largest_image->HasPaintTime()) {
      UpdateWebExposedLargestContentfulImage(*largest_image);
    }
  } else {
    if (text_size > largest_reported_size_ && largest_text->HasPaintTime()) {
      UpdateWebExposedLargestContentfulText(*largest_text);
    }
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulImage(
    const ImageRecord& largest_image) {
  DCHECK(window_performance_);
  const MediaTiming* media_timing = largest_image.GetMediaTiming();
  Node* image_node = largest_image.GetNode();

  // |media_timing| is a weak pointer, so it may be null. This can only happen
  // if the image has been removed, which means that the largest image is not
  // up-to-date. This can happen when this method call came from
  // OnLargestTextUpdated(). If a largest-image is added and removed so fast
  // that it does not get to be reported here, we consider it safe to ignore.
  // For similar reasons, |image_node| may be null and it is safe to ignore
  // the |largest_image| content in this case as well.
  if (!media_timing || !image_node) {
    return;
  }

  largest_image_bpp_ = largest_image.EntropyForLCP();
  largest_reported_size_ = largest_image.RecordedSize();
  const KURL& url = media_timing->Url();
  const String& image_string = url.GetString();
  const String& image_url =
      url.ProtocolIsData()
          ? image_string.Left(ImageElementTiming::kInlineImageMaxChars)
          : image_string;
  // Do not expose element attribution from shadow trees.
  Element* image_element =
      image_node->IsInShadowTree() ? nullptr : To<Element>(image_node);
  const AtomicString& image_id =
      image_element ? image_element->GetIdAttribute() : AtomicString();

  delegate_->EmitLcpPerformanceEntry(
      largest_image.PaintTimingInfo(),
      /*paint_size=*/largest_image.RecordedSize(),
      /*load_time=*/largest_image.LoadTime(),
      /*id=*/image_id, /*url=*/image_url,
      /*element=*/image_element);

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        kTraceCategories,
        delegate_->IsHardNavigation() ? kLCPCandidate
                                      : kLCPCandidateForSoftNavs,
        largest_image.PaintTime(), "data",
        CreateWebExposedCandidateTraceData(largest_image), "frame",
        GetFrameIdForTracing(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulText(
    const TextRecord& largest_text) {
  DCHECK(window_performance_);
  Node* text_node = largest_text.GetNode();
  // |text_node| could be null and |largest_text| should be ignored in this
  // case. This can happen when the largest-text gets removed too fast and does
  // not get to be reported here.
  if (!text_node) {
    return;
  }
  largest_reported_size_ = largest_text.RecordedSize();
  // Do not expose element attribution from shadow trees. Also note that @page
  // margin boxes do not create Element nodes.
  Element* text_element =
      text_node->IsInShadowTree() ? nullptr : DynamicTo<Element>(text_node);
  const AtomicString& text_id =
      text_element ? text_element->GetIdAttribute() : AtomicString();

  // Always use paint time as start time for text LCP candidate.
  delegate_->EmitLcpPerformanceEntry(largest_text.PaintTimingInfo(),
                                     /*paint_size=*/largest_text.RecordedSize(),
                                     /*load_time=*/base::TimeTicks(),
                                     /*id=*/text_id,
                                     /*url=*/g_empty_string,
                                     /*element=*/text_element);

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        kTraceCategories,
        delegate_->IsHardNavigation() ? kLCPCandidate
                                      : kLCPCandidateForSoftNavs,
        largest_text.PaintTime(), "data",
        CreateWebExposedCandidateTraceData(largest_text), "frame",
        GetFrameIdForTracing(window->GetFrame()));
  }
}

bool LargestContentfulPaintCalculator::HasLargestImagePaintChangedForMetrics(
    base::TimeTicks largest_image_paint_time,
    uint64_t largest_image_paint_size) const {
  return largest_image_paint_time !=
             latest_lcp_details_.largest_image_paint_time ||
         largest_image_paint_size !=
             latest_lcp_details_.largest_image_paint_size;
}

bool LargestContentfulPaintCalculator::HasLargestTextPaintChangedForMetrics(
    base::TimeTicks largest_text_paint_time,
    uint64_t largest_text_paint_size) const {
  return largest_text_paint_time !=
             latest_lcp_details_.largest_text_paint_time ||
         largest_text_paint_size != latest_lcp_details_.largest_text_paint_size;
}

bool LargestContentfulPaintCalculator::NotifyMetricsIfLargestImagePaintChanged(
    const ImageRecord& image_record) {
  // TODO(crbug.com/449779010): Unify these.
  base::TimeTicks image_paint_time =
      delegate_->IsHardNavigation() && image_record.HasFirstAnimatedFrameTime()
          ? image_record.FirstAnimatedFrameTime()
          : image_record.PaintTime();

  if (!HasLargestImagePaintChangedForMetrics(image_paint_time,
                                             image_record.RecordedSize())) {
    return false;
  }

  latest_lcp_details_.largest_contentful_paint_type =
      blink::LargestContentfulPaintType::kNone;
  // TODO(yoav): Once we'd enable the kLCPAnimatedImagesReporting flag by
  // default, we'd be able to use the value of
  // largest_image_record->first_animated_frame_time directly.
  if (const MediaTiming* timing = image_record.GetMediaTiming()) {
    if (!timing->GetFirstVideoFrameTime().is_null()) {
      // Set the video flag.
      latest_lcp_details_.largest_contentful_paint_type |=
          blink::LargestContentfulPaintType::kVideo;
    } else if (timing->IsPaintedFirstFrame()) {
      // Set the animated image flag.
      latest_lcp_details_.largest_contentful_paint_type |=
          blink::LargestContentfulPaintType::kAnimatedImage;
    }

    // Set image type flag.
    latest_lcp_details_.largest_contentful_paint_type |=
        blink::LargestContentfulPaintType::kImage;

    // Set specific type of the image.
    latest_lcp_details_.largest_contentful_paint_type |=
        GetLargestContentfulPaintTypeFromString(timing->MediaType());

    // Set DataURI type.
    if (timing->IsDataUrl()) {
      latest_lcp_details_.largest_contentful_paint_type |=
          blink::LargestContentfulPaintType::kDataURI;
    }

    // Set cross-origin flag of the image.
    if (auto* window = window_performance_->DomWindow()) {
      auto image_url = timing->Url();
      if (!image_url.IsEmpty() && image_url.ProtocolIsInHTTPFamily() &&
          window->GetFrame()->IsOutermostMainFrame()) {
        auto image_origin = SecurityOrigin::Create(image_url);
        if (!image_origin->IsSameOriginWith(window->GetSecurityOrigin())) {
          latest_lcp_details_.largest_contentful_paint_type |=
              blink::LargestContentfulPaintType::kCrossOrigin;
        }
      }
    }

    latest_lcp_details_.resource_load_timings.discovery_time =
        timing->DiscoveryTime();
    latest_lcp_details_.resource_load_timings.load_start = timing->LoadStart();
    latest_lcp_details_.resource_load_timings.load_end = timing->LoadEnd();
  }
  latest_lcp_details_.largest_image_paint_time = image_paint_time;
  latest_lcp_details_.largest_image_paint_size = image_record.RecordedSize();
  latest_lcp_details_.largest_contentful_paint_image_bpp =
      image_record.EntropyForLCP();
  latest_lcp_details_.largest_contentful_paint_image_request_priority =
      image_record.RequestPriority();
  UpdateLatestLcpDetailsTypeIfNeeded();

  // TODO(crbug.com/449779010): Consider removing this IsLoaded(), since we
  // having presentation time is the only thing that matters for metrics.  When
  // ReportFirstFrameTimeAsRenderTime ships, we will emit some performance
  // entries before they are considered fully loaded, so this should probably be
  // removed along with shipping that feature.
  if (delegate_->IsHardNavigation() && PaintTimingDetector::IsTracing()) {
    if (!image_paint_time.is_null() && image_record.IsLoaded()) {
      ReportMetricsCandidateToTrace(image_record, image_paint_time);
    } else {
      ReportNoMetricsImageCandidateToTrace();
    }
  }

  return true;
}

bool LargestContentfulPaintCalculator::NotifyMetricsIfLargestTextPaintChanged(
    const TextRecord& text_record) {
  if (!HasLargestTextPaintChangedForMetrics(text_record.PaintTime(),
                                            text_record.RecordedSize())) {
    return false;
  }

  DCHECK(text_record.HasPaintTime());
  latest_lcp_details_.largest_text_paint_time = text_record.PaintTime();
  latest_lcp_details_.largest_text_paint_size = text_record.RecordedSize();
  UpdateLatestLcpDetailsTypeIfNeeded();

  if (delegate_->IsHardNavigation() && PaintTimingDetector::IsTracing()) {
    ReportMetricsCandidateToTrace(text_record);
  }

  return true;
}

void LargestContentfulPaintCalculator::UpdateLatestLcpDetailsTypeIfNeeded() {
  if (latest_lcp_details_.largest_text_paint_size <
          latest_lcp_details_.largest_image_paint_size ||
      (latest_lcp_details_.largest_text_paint_size ==
           latest_lcp_details_.largest_image_paint_size &&
       latest_lcp_details_.largest_text_paint_time >=
           latest_lcp_details_.largest_image_paint_time)) {
    return;
  }
  // We set latest_lcp_details_.largest_contentful_paint_type_ only here
  // because we use latest_lcp_details_.largest_contentful_paint_type_ to
  // track the LCP type of the largest image only. When the largest image gets
  // updated, the latest_lcp_details_.largest_contentful_paint_type_ gets
  // reset and updated accordingly in the
  // NotifyMetricsIfLargestImagePaintChanged() method. If the LCP element
  // turns out to be the largest text, we simply set the
  // latest_lcp_details_.largest_contentful_paint_type_ to be kText here. This
  // is possible because currently text elements have only 1 LCP type kText.
  latest_lcp_details_.largest_contentful_paint_type =
      LargestContentfulPaintType::kText;
}
void LargestContentfulPaintCalculator::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
  visitor->Trace(delegate_);
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::CreateWebExposedCandidateTraceData(
    const TextRecord& largest_text) {
  std::unique_ptr<TracedValue> value =
      CreateWebExposedCandidateTraceDataCommon(largest_text);
  value->SetString("type", "text");
  return value;
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::CreateWebExposedCandidateTraceData(
    const ImageRecord& largest_image) {
  std::unique_ptr<TracedValue> value =
      CreateWebExposedCandidateTraceDataCommon(largest_image);
  value->SetString("type", "image");
  value->SetDouble("imageDiscoveryTime",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image.GetMediaTiming()->DiscoveryTime()));
  value->SetDouble("imageLoadStart",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image.GetMediaTiming()->LoadStart()));
  value->SetDouble("imageLoadEnd",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image.GetMediaTiming()->LoadEnd()));
  if (auto* html_image_element =
          DynamicTo<HTMLImageElement>(largest_image.GetNode())) {
    const AtomicString& loadingAttr =
        html_image_element->FastGetAttribute(html_names::kLoadingAttr);
    value->SetString("loadingAttr", loadingAttr);
  }
  return value;
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::CreateWebExposedCandidateTraceDataCommon(
    const PaintTimingRecord& record) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("nodeId", record.NodeIdForTracing());
  value->SetInteger("size", static_cast<int>(record.RecordedSize()));
  value->SetInteger("candidateIndex", ++web_exposed_candidate_count_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  if (delegate_->IsHardNavigation()) {
    value->SetString("navigationId", IdentifiersFactory::LoaderId(
                                         window->document()->Loader()));
  }
  value->SetInteger("performanceTimelineNavigationId",
                    window_performance_->NavigationId());
  if (Node* node = record.GetNode()) {
    value->SetString("nodeName", node->DebugName());
  }
  return value;
}

void LargestContentfulPaintCalculator::ReportMetricsCandidateToTrace(
    const ImageRecord& record,
    base::TimeTicks time) {
  CHECK(!time.is_null());

  auto value = std::make_unique<TracedValue>();
  record.PopulateTraceValue(*value);
  value->SetInteger("candidateIndex", ++ukm_largest_image_candidate_count_);

  LocalFrame* frame = window_performance_->DomWindow()->GetFrame();
  CHECK(frame);
  PopulateFrameTraceData(*value, *frame);

  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestImagePaint::Candidate",
                                   time, "data", std::move(value), "frame",
                                   GetFrameIdForTracing(frame));
}

void LargestContentfulPaintCalculator::ReportMetricsCandidateToTrace(
    const TextRecord& record) {
  auto value = std::make_unique<TracedValue>();
  record.PopulateTraceValue(*value);
  value->SetInteger("candidateIndex", ++ukm_largest_text_candidate_count_);

  LocalFrame* frame = window_performance_->DomWindow()->GetFrame();
  CHECK(frame);
  PopulateFrameTraceData(*value, *frame);

  CHECK(record.HasPaintTime());
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestTextPaint::Candidate",
                                   record.PaintTime(), "data", std::move(value),
                                   "frame", GetFrameIdForTracing(frame));
}

void LargestContentfulPaintCalculator::ReportNoMetricsImageCandidateToTrace() {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++ukm_largest_image_candidate_count_);

  LocalFrame* frame = window_performance_->DomWindow()->GetFrame();
  CHECK(frame);
  PopulateFrameTraceData(*value, *frame);

  TRACE_EVENT2("loading", "LargestImagePaint::NoCandidate", "data",
               std::move(value), "frame", GetFrameIdForTracing(frame));
}

}  // namespace blink
