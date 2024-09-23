// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"

#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"

namespace blink {
namespace {
std::optional<base::TimeTicks> MergeLargestContentfulPaintValues(
    const LargestContentfulPaintDetails& timing) {
  const uint64_t text_paint_size = timing.largest_text_paint_size;
  const uint64_t image_paint_size = timing.largest_image_paint_size;
  if (text_paint_size == 0 && image_paint_size == 0) {
    return std::nullopt;
  }

  const base::TimeTicks largest_text_paint = timing.largest_text_paint_time;
  const base::TimeTicks largest_image_paint = timing.largest_image_paint_time;

  if (text_paint_size == image_paint_size) {
    return std::min(largest_text_paint, largest_image_paint);
  }
  return text_paint_size > image_paint_size ? largest_text_paint
                                            : largest_image_paint;
}
}  // namespace

static uint64_t ToIntegerMilliseconds(base::TimeDelta duration,
                                      bool cross_origin_isolated_capability) {
  // TODO(npm): add histograms to understand when/why |duration| is sometimes
  // negative.
  // TODO(crbug.com/1063989): stop clamping when it is not needed (i.e. for
  // methods which do not expose the timestamp to a web perf API).
  return static_cast<uint64_t>(Performance::ClampTimeResolution(
      duration, cross_origin_isolated_capability));
}

LargestContentfulPaintDetailsForReporting PerformanceTimingForReporting::
    PopulateLargestContentfulPaintDetailsForReporting(
        const LargestContentfulPaintDetails& timing) const {
  // The largest_image_paint_time and the largest_text_paint_time are converted
  // into seconds.
  double largest_image_paint_time =
      base::Milliseconds(
          MonotonicTimeToIntegerMilliseconds(timing.largest_image_paint_time))
          .InSecondsF();

  double largest_text_paint_time =
      base::Milliseconds(
          MonotonicTimeToIntegerMilliseconds(timing.largest_text_paint_time))
          .InSecondsF();

  ResourceLoadTimingsForReporting resource_load_timings = {
      MonotonicTimeToPseudoWallTime(
          timing.resource_load_timings.discovery_time),
      MonotonicTimeToPseudoWallTime(timing.resource_load_timings.load_start),
      MonotonicTimeToPseudoWallTime(timing.resource_load_timings.load_end)

  };

  std::optional<base::TimeTicks> merged_unclamped_paint_time =
      MergeLargestContentfulPaintValues(timing);

  return {largest_image_paint_time,
          timing.largest_image_paint_size,
          resource_load_timings,
          timing.largest_contentful_paint_type,

          timing.largest_contentful_paint_image_bpp,
          largest_text_paint_time,
          timing.largest_text_paint_size,
          timing.largest_contentful_paint_time,

          timing.largest_contentful_paint_image_request_priority,
          merged_unclamped_paint_time};
}

PerformanceTimingForReporting::PerformanceTimingForReporting(
    ExecutionContext* context)
    : ExecutionContextClient(context) {
  cross_origin_isolated_capability_ =
      context && context->CrossOriginIsolatedCapability();
}

uint64_t PerformanceTimingForReporting::inputStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->InputStart());
}

base::TimeTicks PerformanceTimingForReporting::NavigationStartAsMonotonicTime()
    const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return base::TimeTicks();

  return timing->NavigationStart();
}

PerformanceTimingForReporting::BackForwardCacheRestoreTimings
PerformanceTimingForReporting::BackForwardCacheRestore() const {
  DocumentLoadTiming* load_timing = GetDocumentLoadTiming();
  if (!load_timing)
    return {};

  const PaintTiming* paint_timing = GetPaintTiming();
  if (!paint_timing)
    return {};

  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return {};

  WTF::Vector<base::TimeTicks> navigation_starts =
      load_timing->BackForwardCacheRestoreNavigationStarts();
  WTF::Vector<base::TimeTicks> first_paints =
      paint_timing->FirstPaintsAfterBackForwardCacheRestore();
  WTF::Vector<std::array<
      base::TimeTicks,
      WebPerformanceMetricsForReporting::
          kRequestAnimationFramesToRecordAfterBackForwardCacheRestore>>
      request_animation_frames =
          paint_timing->RequestAnimationFramesAfterBackForwardCacheRestore();
  WTF::Vector<std::optional<base::TimeDelta>> first_input_delays =
      interactive_detector->GetFirstInputDelaysAfterBackForwardCacheRestore();
  DCHECK_EQ(navigation_starts.size(), first_paints.size());
  DCHECK_EQ(navigation_starts.size(), request_animation_frames.size());
  DCHECK_EQ(navigation_starts.size(), first_input_delays.size());

  WTF::Vector<BackForwardCacheRestoreTiming> restore_timings(
      navigation_starts.size());
  for (wtf_size_t i = 0; i < restore_timings.size(); i++) {
    restore_timings[i].navigation_start =
        MonotonicTimeToIntegerMilliseconds(navigation_starts[i]);
    restore_timings[i].first_paint =
        MonotonicTimeToIntegerMilliseconds(first_paints[i]);
    for (wtf_size_t j = 0; j < request_animation_frames[i].size(); j++) {
      restore_timings[i].request_animation_frames[j] =
          MonotonicTimeToIntegerMilliseconds(request_animation_frames[i][j]);
    }
    restore_timings[i].first_input_delay = first_input_delays[i];
  }
  return restore_timings;
}

uint64_t PerformanceTimingForReporting::FirstPaintForMetrics() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstPaintForMetrics());
}

uint64_t PerformanceTimingForReporting::FirstImagePaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstImagePaint());
}

uint64_t
PerformanceTimingForReporting::FirstContentfulPaintIgnoringSoftNavigations()
    const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->FirstContentfulPaintIgnoringSoftNavigations());
}

base::TimeTicks PerformanceTimingForReporting::
    FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return base::TimeTicks();

  return timing->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
}

base::TimeTicks
PerformanceTimingForReporting::FirstContentfulPaintAsMonotonicTimeForMetrics()
    const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return base::TimeTicks();

  return timing->FirstContentfulPaintIgnoringSoftNavigations();
}

uint64_t PerformanceTimingForReporting::FirstMeaningfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstMeaningfulPaint());
}

uint64_t PerformanceTimingForReporting::FirstMeaningfulPaintCandidate() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->FirstMeaningfulPaintCandidate());
}

LargestContentfulPaintDetailsForReporting
PerformanceTimingForReporting::LargestContentfulPaintDetailsForMetrics() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector) {
    return {};
  }

  auto timing =
      paint_timing_detector->LargestContentfulPaintDetailsForMetrics();

  return PopulateLargestContentfulPaintDetailsForReporting(timing);
}

LargestContentfulPaintDetailsForReporting PerformanceTimingForReporting::
    SoftNavigationLargestContentfulPaintDetailsForMetrics() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector) {
    return {};
  }

  auto timing = paint_timing_detector
                    ->SoftNavigationLargestContentfulPaintDetailsForMetrics();

  return PopulateLargestContentfulPaintDetailsForReporting(timing);
}

uint64_t PerformanceTimingForReporting::FirstEligibleToPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing) {
    return 0;
  }

  return MonotonicTimeToIntegerMilliseconds(timing->FirstEligibleToPaint());
}

uint64_t PerformanceTimingForReporting::FirstInputOrScrollNotifiedTimestamp()
    const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->FirstInputOrScrollNotifiedTimestamp());
}

std::optional<base::TimeDelta> PerformanceTimingForReporting::FirstInputDelay()
    const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return std::nullopt;

  return interactive_detector->GetFirstInputDelay();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::FirstInputTimestamp() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return std::nullopt;

  return MonotonicTimeToPseudoWallTime(
      interactive_detector->GetFirstInputTimestamp());
}

std::optional<base::TimeTicks>
PerformanceTimingForReporting::FirstInputTimestampAsMonotonicTime() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return std::nullopt;

  return interactive_detector->GetFirstInputTimestamp();
}

std::optional<base::TimeDelta> PerformanceTimingForReporting::FirstScrollDelay()
    const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return std::nullopt;

  return interactive_detector->GetFirstScrollDelay();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::FirstScrollTimestamp() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return std::nullopt;

  return MonotonicTimeToPseudoWallTime(
      interactive_detector->GetFirstScrollTimestamp());
}

uint64_t PerformanceTimingForReporting::ParseStart() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStart());
}

uint64_t PerformanceTimingForReporting::ParseStop() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStop());
}

uint64_t PerformanceTimingForReporting::ParseBlockedOnScriptLoadDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->ParserBlockedOnScriptLoadDuration(),
                               cross_origin_isolated_capability_);
}

uint64_t PerformanceTimingForReporting::
    ParseBlockedOnScriptLoadFromDocumentWriteDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptLoadFromDocumentWriteDuration(),
      cross_origin_isolated_capability_);
}

uint64_t PerformanceTimingForReporting::ParseBlockedOnScriptExecutionDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->ParserBlockedOnScriptExecutionDuration(),
                               cross_origin_isolated_capability_);
}

uint64_t PerformanceTimingForReporting::
    ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionFromDocumentWriteDuration(),
      cross_origin_isolated_capability_);
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::PrerenderActivationStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  base::TimeTicks activation_start = timing->ActivationStart();
  if (activation_start.is_null())
    return std::nullopt;

  return timing->MonotonicTimeToZeroBasedDocumentTime(activation_start);
}

std::optional<base::TimeTicks> PerformanceTimingForReporting::UnloadStart()
    const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->UnloadEventStart();
}

std::optional<base::TimeTicks> PerformanceTimingForReporting::UnloadEnd()
    const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->UnloadEventEnd();
}

std::optional<base::TimeTicks>
PerformanceTimingForReporting::CommitNavigationEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->CommitNavigationEnd();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::UserTimingMarkFullyLoaded() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->UserTimingMarkFullyLoaded();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::UserTimingMarkFullyVisible() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->UserTimingMarkFullyVisible();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::UserTimingMarkInteractive() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->UserTimingMarkInteractive();
}

std::optional<std::tuple<AtomicString, base::TimeDelta>>
PerformanceTimingForReporting::CustomUserTimingMark() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return std::nullopt;
  }

  return timing->CustomUserTimingMark();
}

DocumentLoader* PerformanceTimingForReporting::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->GetFrame()->Loader().GetDocumentLoader()
                     : nullptr;
}

const DocumentTiming* PerformanceTimingForReporting::GetDocumentTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &DomWindow()->document()->GetTiming();
}

const PaintTiming* PerformanceTimingForReporting::GetPaintTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &PaintTiming::From(*DomWindow()->document());
}

const DocumentParserTiming*
PerformanceTimingForReporting::GetDocumentParserTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &DocumentParserTiming::From(*DomWindow()->document());
}

DocumentLoadTiming* PerformanceTimingForReporting::GetDocumentLoadTiming()
    const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

InteractiveDetector* PerformanceTimingForReporting::GetInteractiveDetector()
    const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return InteractiveDetector::From(*DomWindow()->document());
}

PaintTimingDetector* PerformanceTimingForReporting::GetPaintTimingDetector()
    const {
  if (!DomWindow())
    return nullptr;
  return &DomWindow()->GetFrame()->View()->GetPaintTimingDetector();
}

std::optional<base::TimeDelta>
PerformanceTimingForReporting::MonotonicTimeToPseudoWallTime(
    const std::optional<base::TimeTicks>& time) const {
  if (!time.has_value())
    return std::nullopt;

  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return std::nullopt;

  return timing->MonotonicTimeToPseudoWallTime(*time);
}

std::unique_ptr<TracedValue>
PerformanceTimingForReporting::GetNavigationTracingData() {
  auto data = std::make_unique<TracedValue>();
  data->SetString("navigationId",
                  IdentifiersFactory::LoaderId(GetDocumentLoader()));
  return data;
}

uint64_t PerformanceTimingForReporting::MonotonicTimeToIntegerMilliseconds(
    base::TimeTicks time) const {
  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->MonotonicTimeToPseudoWallTime(time),
                               cross_origin_isolated_capability_);
}

void PerformanceTimingForReporting::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
