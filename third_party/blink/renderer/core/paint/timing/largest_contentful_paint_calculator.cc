// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"

#include "base/check.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr const char kTraceCategories[] = "loading,rail,devtools.timeline";

constexpr const char kLCPCandidate[] = "largestContentfulPaint::Candidate";

}  // namespace

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

void LargestContentfulPaintCalculator::
    UpdateWebExposedLargestContentfulPaintIfNeeded(
        const TextRecord* largest_text,
        const ImageRecord* largest_image,
        bool is_triggered_by_soft_navigation) {
  uint64_t text_size = largest_text ? largest_text->recorded_size : 0u;
  uint64_t image_size = largest_image ? largest_image->recorded_size : 0u;
  if (image_size > text_size) {
    if (image_size > largest_reported_size_ &&
        largest_image->paint_time > base::TimeTicks()) {
      UpdateWebExposedLargestContentfulImage(largest_image,
                                             is_triggered_by_soft_navigation);
    }
  } else {
    if (text_size > largest_reported_size_ &&
        largest_text->paint_time > base::TimeTicks()) {
      UpdateWebExposedLargestContentfulText(*largest_text,
                                            is_triggered_by_soft_navigation);
    }
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulImage(
    const ImageRecord* largest_image,
    bool is_triggered_by_soft_navigation) {
  DCHECK(window_performance_);
  DCHECK(largest_image);
  const MediaTiming* media_timing = largest_image->media_timing;
  Node* image_node = DOMNodeIds::NodeForId(largest_image->node_id);

  // |media_timing| is a weak pointer, so it may be null. This can only happen
  // if the image has been removed, which means that the largest image is not
  // up-to-date. This can happen when this method call came from
  // OnLargestTextUpdated(). If a largest-image is added and removed so fast
  // that it does not get to be reported here, we consider it safe to ignore.
  // For similar reasons, |image_node| may be null and it is safe to ignore
  // the |largest_image| content in this case as well.
  if (!media_timing || !image_node)
    return;

  uint64_t size = largest_image->recorded_size;
  double bpp = largest_image->EntropyForLCP();

  if (base::FeatureList::IsEnabled(features::kExcludeLowEntropyImagesFromLCP)) {
    if (bpp < features::kMinimumEntropyForLCP.Get()) {
      return;
    }
  }
  largest_image_bpp_ = bpp;
  largest_reported_size_ = size;
  const KURL& url = media_timing->Url();
  bool expose_paint_time_to_api =
      url.ProtocolIsData() || media_timing->TimingAllowPassed();
  const String& image_url =
      url.ProtocolIsData()
          ? url.GetString().Left(ImageElementTiming::kInlineImageMaxChars)
          : url.GetString();
  // Do not expose element attribution from shadow trees.
  Element* image_element =
      image_node->IsInShadowTree() ? nullptr : To<Element>(image_node);
  const AtomicString& image_id =
      image_element ? image_element->GetIdAttribute() : AtomicString();

  base::TimeTicks start_time = expose_paint_time_to_api
                                   ? largest_image->paint_time
                                   : largest_image->load_time;

  if (RuntimeEnabledFeatures::ExposeRenderTimeNonTaoDelayedImageEnabled() &&
      !expose_paint_time_to_api) {
    // For Non-Tao images, set start time to the max of FCP and load time.
    base::TimeTicks fcp =
        PaintTiming::From(*window_performance_->DomWindow()->document())
            .FirstContentfulPaintPresentation();
    DCHECK(!fcp.is_null());
    start_time = std::max(fcp, largest_image->load_time);
  }
  base::TimeTicks renderTime =
      expose_paint_time_to_api ? largest_image->paint_time : base::TimeTicks();

  window_performance_->OnLargestContentfulPaintUpdated(
      /*start_time=*/start_time, /*render_time=*/renderTime,
      /*paint_size=*/largest_image->recorded_size,
      /*load_time=*/largest_image->load_time,
      /*first_animated_frame_time=*/
      expose_paint_time_to_api ? largest_image->first_animated_frame_time
                               : base::TimeTicks(),
      /*id=*/image_id, /*url=*/image_url, /*element=*/image_element,
      is_triggered_by_soft_navigation);

  // TODO: update trace value with animated frame data
  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    if (!largest_image->origin_clean) {
      UseCounter::Count(window->document(),
                        WebFeature::kLCPCandidateImageFromOriginDirtyStyle);
    }

    TRACE_EVENT_MARK_WITH_TIMESTAMP2(kTraceCategories, kLCPCandidate,
                                     largest_image->paint_time, "data",
                                     ImageCandidateTraceData(largest_image),
                                     "frame", ToTraceValue(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::UpdateWebExposedLargestContentfulText(
    const TextRecord& largest_text,
    bool is_triggered_by_soft_navigation) {
  DCHECK(window_performance_);
  // |node_| could be null and |largest_text| should be ignored in this
  // case. This can happen when the largest-text gets removed too fast and does
  // not get to be reported here.
  if (!largest_text.node_)
    return;
  Node* text_node = largest_text.node_;
  largest_reported_size_ = largest_text.recorded_size;
  // Do not expose element attribution from shadow trees.
  Element* text_element =
      text_node->IsInShadowTree() ? nullptr : To<Element>(text_node);
  const AtomicString& text_id =
      text_element ? text_element->GetIdAttribute() : AtomicString();
  // Always use paint time as start time for text LCP candidate.
  window_performance_->OnLargestContentfulPaintUpdated(
      /*start_time=*/largest_text.paint_time,
      /*render_time=*/largest_text.paint_time,
      /*paint_size=*/largest_text.recorded_size,
      /*load_time=*/base::TimeTicks(),
      /*first_animated_frame_time=*/base::TimeTicks(), /*id=*/text_id,
      /*url=*/g_empty_string, /*element=*/text_element,
      is_triggered_by_soft_navigation);

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(kTraceCategories, kLCPCandidate,
                                     largest_text.paint_time, "data",
                                     TextCandidateTraceData(largest_text),
                                     "frame", ToTraceValue(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::TextCandidateTraceData(
    const TextRecord& largest_text) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "text");
  value->SetInteger(
      "nodeId", static_cast<int>(DOMNodeIds::IdForNode(largest_text.node_)));
  value->SetInteger("size", static_cast<int>(largest_text.recorded_size));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  value->SetString("navigationId",
                   IdentifiersFactory::LoaderId(window->document()->Loader()));
  return value;
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::ImageCandidateTraceData(
    const ImageRecord* largest_image) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "image");
  value->SetInteger("nodeId", static_cast<int>(largest_image->node_id));
  value->SetInteger("size", static_cast<int>(largest_image->recorded_size));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isOutermostMainFrame",
                    window->GetFrame()->IsOutermostMainFrame());
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  value->SetString("navigationId",
                   IdentifiersFactory::LoaderId(window->document()->Loader()));

  return value;
}

}  // namespace blink
