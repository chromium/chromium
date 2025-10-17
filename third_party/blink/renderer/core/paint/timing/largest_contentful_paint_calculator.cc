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
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/navigation_id_generator.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr const char kTraceCategories[] = "loading,rail,devtools.timeline";

constexpr const char kLCPCandidate[] = "largestContentfulPaint::Candidate";

// A fixed navigationId for when we're emitting soft-navs related LCP trace
// events; this is a valid UUIDv4. But, there is no corresponding navigation for
// this ID, and therefore, DevTools will ignore these events.
// TODO: Remove this once we introduce new trace events for soft-navs.
constexpr const char kFixedNavigationIdForSoftNavs[] =
    "deadbeef-dead-beef-dead-beefdeadbeef";

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
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

void LargestContentfulPaintCalculator::
    UpdateWebExposedLargestContentfulPaintIfNeeded(
        const TextRecord* largest_text,
        const ImageRecord* largest_image,
        bool is_triggered_by_soft_navigation,
        uint32_t navigation_id) {
  uint64_t text_size = largest_text ? largest_text->RecordedSize() : 0u;
  uint64_t image_size = largest_image ? largest_image->RecordedSize() : 0u;
  if (image_size > text_size) {
    if (image_size > largest_reported_size_ && largest_image->HasPaintTime()) {
      UpdateWebExposedLargestContentfulImage(
          largest_image, is_triggered_by_soft_navigation, navigation_id);
    }
  } else {
    if (text_size > largest_reported_size_ && largest_text->HasPaintTime()) {
      UpdateWebExposedLargestContentfulText(
          *largest_text, is_triggered_by_soft_navigation, navigation_id);
    }
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulImage(
    const ImageRecord* largest_image,
    bool is_triggered_by_soft_navigation,
    uint32_t navigation_id) {
  DCHECK(window_performance_);
  DCHECK(largest_image);
  const MediaTiming* media_timing = largest_image->GetMediaTiming();
  Node* image_node = largest_image->GetNode();

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

  largest_image_bpp_ = largest_image->EntropyForLCP();
  largest_reported_size_ = largest_image->RecordedSize();
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

  if (!is_triggered_by_soft_navigation) {
    CHECK_EQ(kNavigationIdAbsentValue, navigation_id);
    window_performance_->OnLargestContentfulPaintUpdated(
        std::make_optional(largest_image->PaintTimingInfo()),
        /*paint_size=*/largest_image->RecordedSize(),
        /*load_time=*/largest_image->LoadTime(),
        /*id=*/image_id, /*url=*/image_url, /*element=*/image_element);
  } else {
    window_performance_->OnInteractionContentfulPaintUpdated(
        std::make_optional(largest_image->PaintTimingInfo()),
        /*paint_size=*/largest_image->RecordedSize(),
        /*load_time=*/largest_image->LoadTime(),
        /*id=*/image_id, /*url=*/image_url, /*element=*/image_element,
        /*navigation_id=*/navigation_id);
  }

  // TODO: update trace value with animated frame data
  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    if (!largest_image->IsOriginClean()) {
      UseCounter::Count(window->document(),
                        WebFeature::kLCPCandidateImageFromOriginDirtyStyle);
    }

    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        kTraceCategories, kLCPCandidate, largest_image->PaintTime(), "data",
        ImageCandidateTraceData(largest_image, is_triggered_by_soft_navigation),
        "frame", GetFrameIdForTracing(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulText(
    const TextRecord& largest_text,
    bool is_triggered_by_soft_navigation,
    uint32_t navigation_id) {
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
  if (!is_triggered_by_soft_navigation) {
    CHECK_EQ(kNavigationIdAbsentValue, navigation_id);
    window_performance_->OnLargestContentfulPaintUpdated(
        largest_text.PaintTimingInfo(),
        /*paint_size=*/largest_text.RecordedSize(),
        /*load_time=*/base::TimeTicks(),
        /*id=*/text_id,
        /*url=*/g_empty_string, /*element=*/text_element);
  } else {
    window_performance_->OnInteractionContentfulPaintUpdated(
        largest_text.PaintTimingInfo(),
        /*paint_size=*/largest_text.RecordedSize(),
        /*load_time=*/base::TimeTicks(),
        /*id=*/text_id,
        /*url=*/g_empty_string, /*element=*/text_element,
        /*navigation_id=*/navigation_id);
  }

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        kTraceCategories, kLCPCandidate, largest_text.PaintTime(), "data",
        TextCandidateTraceData(largest_text, is_triggered_by_soft_navigation),
        "frame", GetFrameIdForTracing(window->GetFrame()));
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
    base::TimeTicks image_paint_time,
    uint64_t image_paint_size,
    ImageRecord* image_record,
    double image_bpp,
    std::optional<WebURLRequest::Priority> priority) {
  if (!HasLargestImagePaintChangedForMetrics(image_paint_time,
                                             image_paint_size)) {
    return false;
  }

  latest_lcp_details_.largest_contentful_paint_type =
      blink::LargestContentfulPaintType::kNone;
  if (image_record) {
    // TODO(yoav): Once we'd enable the kLCPAnimatedImagesReporting flag by
    // default, we'd be able to use the value of
    // largest_image_record->first_animated_frame_time directly.
    if (image_record && image_record->GetMediaTiming()) {
      if (!image_record->GetMediaTiming()->GetFirstVideoFrameTime().is_null()) {
        // Set the video flag.
        latest_lcp_details_.largest_contentful_paint_type |=
            blink::LargestContentfulPaintType::kVideo;
      } else if (image_record->GetMediaTiming()->IsPaintedFirstFrame()) {
        // Set the animated image flag.
        latest_lcp_details_.largest_contentful_paint_type |=
            blink::LargestContentfulPaintType::kAnimatedImage;
      }

      // Set image type flag.
      latest_lcp_details_.largest_contentful_paint_type |=
          blink::LargestContentfulPaintType::kImage;

      // Set specific type of the image.
      latest_lcp_details_.largest_contentful_paint_type |=
          GetLargestContentfulPaintTypeFromString(
              image_record->GetMediaTiming()->MediaType());

      // Set DataURI type.
      if (image_record->GetMediaTiming()->IsDataUrl()) {
        latest_lcp_details_.largest_contentful_paint_type |=
            blink::LargestContentfulPaintType::kDataURI;
      }

      // Set cross-origin flag of the image.
      if (auto* window = window_performance_->DomWindow()) {
        auto image_url = image_record->GetMediaTiming()->Url();
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
          image_record->GetMediaTiming()->DiscoveryTime();
      latest_lcp_details_.resource_load_timings.load_start =
          image_record->GetMediaTiming()->LoadStart();
      latest_lcp_details_.resource_load_timings.load_end =
          image_record->GetMediaTiming()->LoadEnd();
    }
  }
  latest_lcp_details_.largest_image_paint_time = image_paint_time;
  latest_lcp_details_.largest_image_paint_size = image_paint_size;
  latest_lcp_details_.largest_contentful_paint_image_bpp = image_bpp;
  latest_lcp_details_.largest_contentful_paint_image_request_priority =
      std::move(priority);
  UpdateLatestLcpDetailsTypeIfNeeded();
  return true;
}

bool LargestContentfulPaintCalculator::NotifyMetricsIfLargestTextPaintChanged(
    base::TimeTicks text_paint_time,
    uint64_t text_paint_size) {
  if (!HasLargestTextPaintChangedForMetrics(text_paint_time, text_paint_size)) {
    return false;
  }

  DCHECK(!text_paint_time.is_null());
  latest_lcp_details_.largest_text_paint_time = text_paint_time;
  latest_lcp_details_.largest_text_paint_size = text_paint_size;
  UpdateLatestLcpDetailsTypeIfNeeded();

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
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::TextCandidateTraceData(
    const TextRecord& largest_text,
    bool is_triggered_by_soft_navigation) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "text");
  value->SetInteger("nodeId", largest_text.NodeIdForTracing());
  value->SetInteger("size", static_cast<int>(largest_text.RecordedSize()));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  // Set navigationId to this fixed string for soft navs, to avoid that the
  // event gets associated with the hard navigation (e.g., in DevTools).
  value->SetString("navigationId", is_triggered_by_soft_navigation
                                       ? kFixedNavigationIdForSoftNavs
                                       : IdentifiersFactory::LoaderId(
                                             window->document()->Loader()));
  // TODO(crbug.com/426595418): Clean up this field once we support an
  // event for soft lcp to be issued (Interaction Contentful Paint).
  value->SetInteger("performanceTimelineNavigationId",
                    window_performance_->NavigationId());

  if (Node* node = largest_text.GetNode()) {
    value->SetString("nodeName", node->DebugName());
  }

  return value;
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::ImageCandidateTraceData(
    const ImageRecord* largest_image,
    bool is_triggered_by_soft_navigation) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "image");
  value->SetInteger("nodeId", largest_image->NodeIdForTracing());
  value->SetInteger("size", static_cast<int>(largest_image->RecordedSize()));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  // Set navigationId to this fixed string for soft navs, to avoid that the
  // event gets associated with the hard navigation (e.g., in DevTools).
  value->SetString("navigationId", is_triggered_by_soft_navigation
                                       ? kFixedNavigationIdForSoftNavs
                                       : IdentifiersFactory::LoaderId(
                                             window->document()->Loader()));
  // TODO(crbug.com/426595418): Clean up this field once we support an
  // event for soft lcp to be issued (Interaction Contentful Paint).
  value->SetInteger("performanceTimelineNavigationId",
                    window_performance_->NavigationId());
  value->SetDouble("imageDiscoveryTime",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image->GetMediaTiming()->DiscoveryTime()));
  value->SetDouble("imageLoadStart",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image->GetMediaTiming()->LoadStart()));
  value->SetDouble("imageLoadEnd",
                   window_performance_->MonotonicTimeToDOMHighResTimeStamp(
                       largest_image->GetMediaTiming()->LoadEnd()));

  if (Node* node = largest_image->GetNode()) {
    value->SetString("nodeName", node->DebugName());
    if (auto* html_image_element = DynamicTo<HTMLImageElement>(node)) {
      const AtomicString& loadingAttr =
          html_image_element->FastGetAttribute(html_names::kLoadingAttr);
      value->SetString("loadingAttr", loadingAttr);
    }
  }

  return value;
}

}  // namespace blink
