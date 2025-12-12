// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

WindowPerformance* GetPerformanceInstance(LocalFrame* frame) {
  WindowPerformance* performance = nullptr;
  if (frame && frame->DomWindow()) {
    performance = DOMWindowPerformance::performance(*frame->DomWindow());
  }
  return performance;
}

struct PendingPaintTimingRecord {
  HashSet<PaintEvent> paint_events;
  base::TimeTicks rendering_update_end_time;
};

}  // namespace

class RecodingTimeAfterBackForwardCacheRestoreFrameCallback
    : public FrameCallback {
 public:
  RecodingTimeAfterBackForwardCacheRestoreFrameCallback(
      PaintTiming* paint_timing,
      wtf_size_t record_index)
      : paint_timing_(paint_timing), record_index_(record_index) {}
  ~RecodingTimeAfterBackForwardCacheRestoreFrameCallback() override = default;

  void Invoke(double high_res_time_ms) override {
    // Instead of |high_res_time_ms|, use PaintTiming's |clock_->NowTicks()| for
    // consistency and testability.
    paint_timing_->SetRequestAnimationFrameAfterBackForwardCacheRestore(
        record_index_, count_);

    count_++;
    if (count_ ==
        WebPerformanceMetricsForReporting::
            kRequestAnimationFramesToRecordAfterBackForwardCacheRestore) {
      paint_timing_->NotifyPaintTimingChanged();
      return;
    }

    if (auto* frame = paint_timing_->GetFrame()) {
      if (auto* document = frame->GetDocument()) {
        document->RequestAnimationFrame(this);
      }
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(paint_timing_);
    FrameCallback::Trace(visitor);
  }

 private:
  Member<PaintTiming> paint_timing_;
  const wtf_size_t record_index_;
  size_t count_ = 0;
};

// static
const char PaintTiming::kSupplementName[] = "PaintTiming";

// static
PaintTiming& PaintTiming::From(Document& document) {
  PaintTiming* timing = Supplement<Document>::From<PaintTiming>(document);
  if (!timing) {
    timing = MakeGarbageCollected<PaintTiming>(document);
    ProvideTo(document, timing);
  }
  return *timing;
}

// static
const PaintTiming* PaintTiming::From(const Document& document) {
  PaintTiming* timing = Supplement<Document>::From<PaintTiming>(document);
  return timing;
}

void PaintTiming::MarkFirstPaint() {
  // Test that |first_paint_| is non-zero here, as well as in
  // setFirstPaint, so we avoid invoking monotonicallyIncreasingTime() on every
  // call to markFirstPaint().
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  if (!relevant_paint_details.first_paint_.is_null()) {
    return;
  }
  DCHECK_EQ(IgnorePaintTimingScope::IgnoreDepth(), 0);
  SetFirstPaint(clock_->NowTicks());
  pending_paint_events_.insert(PaintEvent::kFirstPaint);
}

void PaintTiming::MarkFirstContentfulPaint() {
  // Test that |first_contentful_paint_| is non-zero here, as
  // well as in SetFirstContentfulPaint, so we avoid invoking
  // MonotonicallyIncreasingTime() on every call to
  // MarkFirstContentfulPaint().
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  if (!relevant_paint_details.first_contentful_paint_.is_null()) {
    return;
  }
  if (IgnorePaintTimingScope::IgnoreDepth() > 0)
    return;
  SetFirstContentfulPaint(clock_->NowTicks());
}

void PaintTiming::MarkFirstImagePaint() {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  if (!relevant_paint_details.first_image_paint_.is_null()) {
    return;
  }
  DCHECK_EQ(IgnorePaintTimingScope::IgnoreDepth(), 0);
  relevant_paint_details.first_image_paint_ = clock_->NowTicks();
  SetFirstContentfulPaint(relevant_paint_details.first_image_paint_);
  Mark(PaintEvent::kFirstImagePaint);
}

void PaintTiming::MarkFirstEligibleToPaint() {
  if (!first_eligible_to_paint_.is_null())
    return;

  first_eligible_to_paint_ = clock_->NowTicks();
  NotifyPaintTimingChanged();
}

// We deliberately use |paint_details_.first_paint_| here rather than
// |paint_details_.first_paint_presentation_|, because
// |paint_details_.first_paint_presentation_| is set asynchronously and we need
// to be able to rely on a synchronous check that SetFirstPaintPresentation
// hasn't been scheduled or run.
void PaintTiming::MarkIneligibleToPaint() {
  if (first_eligible_to_paint_.is_null() ||
      !paint_details_.first_paint_.is_null()) {
    return;
  }

  first_eligible_to_paint_ = base::TimeTicks();
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstMeaningfulPaintCandidate(base::TimeTicks timestamp) {
  if (!first_meaningful_paint_candidate_.is_null())
    return;
  first_meaningful_paint_candidate_ = timestamp;
  if (GetFrame() && GetFrame()->View() && !GetFrame()->View()->IsAttached()) {
    GetFrame()->GetFrameScheduler()->OnFirstMeaningfulPaint();
  }
}

void PaintTiming::SetFirstMeaningfulPaint(
    base::TimeTicks presentation_time,
    FirstMeaningfulPaintDetector::HadUserInput had_input) {
  DCHECK(first_meaningful_paint_presentation_.is_null());
  DCHECK(!presentation_time.is_null());

  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading,rail,devtools.timeline",
                                   "firstMeaningfulPaint", presentation_time,
                                   "frame", GetFrameIdForTracing(GetFrame()),
                                   "afterUserInput", had_input);

  // Notify FMP for UMA only if there's no user input before FMP, so that layout
  // changes caused by user interactions wouldn't be considered as FMP.
  if (had_input == FirstMeaningfulPaintDetector::kNoUserInput) {
    first_meaningful_paint_presentation_ = presentation_time;
    NotifyPaintTimingChanged();
  }
}

void PaintTiming::NotifyPaint(bool is_first_paint,
                              bool text_painted,
                              bool image_painted) {
  if (IgnorePaintTimingScope::IgnoreDepth() > 0)
    return;

  if (is_first_paint)
    MarkFirstPaint();
  if (text_painted)
    MarkFirstContentfulPaint();
  if (image_painted)
    MarkFirstImagePaint();
  fmp_detector_->NotifyPaint();

  if (is_first_paint)
    GetFrame()->OnFirstPaint(text_painted, image_painted);
}

// https://w3c.github.io/paint-timing/#mark-paint-timing
void PaintTiming::MarkPaintTiming() {
  // 2. Let paintTimingInfo be a new paint timing info, whose rendering update
  // end time is the current high resolution time given document’s relevant
  // global object.
  last_rendering_update_end_time_ = base::TimeTicks::Now();
  // This continues in MarkPaintTimingInternal(), once we've received the paint
  // callbacks.
}

void PaintTiming::MarkPaintTimingInternal() {
  PaintTimingDetector* detector = &GetFrame()->View()->GetPaintTimingDetector();
  SoftNavigationHeuristics* soft_navigation_heuristics =
      GetFrame()->DomWindow()->GetSoftNavigationHeuristics();

  // 3. Let paintedImages be a new ordered set...
  auto add_painted_images_element_timing_entries =
      ImageElementTiming::From(*GetFrame()->DomWindow())
          .TakePaintTimingCallback();
  // 4. Let paintedTextNodes be a new ordered set
  auto add_painted_text_entries =
      detector->GetTextPaintTimingDetector().TakePaintTimingCallback();

  // TODO(crbug.com/381270287) expose PaintTiming also for LCP, and ensure
  // entries are queued in spec order.
  auto add_image_lcp_entries =
      detector->GetImagePaintTimingDetector().TakePaintTimingCallback();

  // 7. Let reportedPaints be the document’s set of previously reported paints.
  PendingPaintTimingRecord paint_timing_record{
      .paint_events = pending_paint_events_,
      .rendering_update_end_time = last_rendering_update_end_time_};
  pending_paint_events_.clear();

  FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
  if (!widget) {
    return;
  }

  // 8. Let frameTimingInfo be document’s current frame timing info.
  // 9. Set document’s current frame timing info to null.
  // (RecordRenderingUpdateEndTime resets the |current frame timing info| inside
  // AnimationFrameTimingMonitor, so it reads a bit different from the spec).
  AnimationFrameTimingInfo* frame_timing_info =
      GetFrame()->IsLocalRoot() ? widget->RecordRenderingUpdateEndTime(
                                      last_rendering_update_end_time_)
                                : nullptr;

  if (paint_timing_record.paint_events.empty() && !frame_timing_info &&
      !add_painted_images_element_timing_entries && !add_painted_text_entries &&
      !add_image_lcp_entries) {
    return;
  }

  // 10. Let flushPaintTimings be the following steps:
  PaintTimingCallback flush_paint_timings =
      blink::BindOnce(
          [](WindowPerformance* performance,
             const PendingPaintTimingRecord& record,
             AnimationFrameTimingInfo* frame_timing_info,
             OptionalPaintTimingCallback image_lcp_callback,
             OptionalPaintTimingCallback painted_images_callback,
             OptionalPaintTimingCallback painted_text_callback,
             PaintTimingDetector* paint_timing_detector,
             SoftNavigationHeuristics* soft_navigation_heuristics,
             const base::TimeTicks& raw_presentation_timestamp,
             const DOMPaintTimingInfo& paint_timing_info) {
            // If the frame was detached between scheduling the coarsening task
            // and running it, do nothing. This matches the non-coarsening case,
            // which already checks detach via `GetPerformanceInstance()`.
            if (!performance || !performance->GetExecutionContext()) {
              return;
            }

            // 10.1. If document should report first paint,
            // then: Report paint timing given document,
            // "first-paint", and paintTimingInfo.
            if (record.paint_events.Contains(PaintEvent::kFirstPaint)) {
              performance->AddFirstPaintTiming(paint_timing_info);
            }

            // 10.2. If document should report first contentful paint,
            // then: Report paint timing given document,
            // "first-contentful-paint", and paintTimingInfo.
            if (record.paint_events.Contains(
                    PaintEvent::kFirstContentfulPaint)) {
              performance->AddFirstContentfulPaintTiming(paint_timing_info);
            }

            // 10.3. Report largest contentful paint given document,
            // paintTimingInfo, paintedImages and paintedTextNodes.
            if (image_lcp_callback) {
              std::move(image_lcp_callback.value())
                  .Run(raw_presentation_timestamp, paint_timing_info);
            }

            const bool may_have_lcp =
                image_lcp_callback || painted_text_callback;

            // 10.4 Report element timing given document, paintTimingInfo,
            // paintedImages and paintedTextNodes.
            if (painted_images_callback) {
              std::move(painted_images_callback.value())
                  .Run(raw_presentation_timestamp, paint_timing_info);
            }
            if (painted_text_callback) {
              std::move(painted_text_callback.value())
                  .Run(raw_presentation_timestamp, paint_timing_info);
            }

            if (paint_timing_detector && may_have_lcp) {
              paint_timing_detector->UpdateLcpCandidate();
            }

            if (soft_navigation_heuristics && may_have_lcp) {
              soft_navigation_heuristics->UpdateSoftLcpCandidate();
            }

            // 10.5 If frameTimingInfo is not null, then queue a long
            // animation frame entry given document, frameTimingInfo, and
            // paintTimingInfo.
            if (frame_timing_info) {
              performance->QueueLongAnimationFrameTiming(frame_timing_info,
                                                         paint_timing_info);
            }
          },
          WrapWeakPersistent(GetPerformanceInstance(GetFrame())),
          paint_timing_record, WrapPersistent(frame_timing_info),
          std::move(add_image_lcp_entries),
          std::move(add_painted_images_element_timing_entries),
          std::move(add_painted_text_entries), WrapWeakPersistent(detector),
          WrapWeakPersistent(soft_navigation_heuristics));

  // 11. If the user-agent does not support implementation-defined presentation
  // times, call flushPaintTimings and return.

  // 12. Run the following steps In parallel:
  // 12.1 Wait until an implementation-defined time when the current frame has
  //    been presented to the user.
  RegisterNotifyPresentationTime(blink::BindOnce(
      [](PaintTiming* self, PaintTimingCallback flush_paint_timings,
         const PendingPaintTimingRecord& record,
         const viz::FrameTimingDetails& frame_timing_details) {
        if (!self) {
          return;
        }

        // Internal presentation times are not over-coarsened as they are not
        // web-exposed.
        for (PaintEvent event : record.paint_events) {
          self->ReportPresentationTime(event, record.rendering_update_end_time,
                                       frame_timing_details);
        }

        WindowPerformance* performance =
            GetPerformanceInstance(self->GetFrame());
        if (!performance) {
          return;
        }

        // 12.2 Set paintTimingInfo’s implementation-defined presentation time
        // to the current high resolution time given document’s relevant global
        // object.
        // (Note: the "current time" is acquired in parallel inside
        // RegisterNotifyPresentationTime, not here).
        // 12.3 If document’s cross-origin isolated capability is false, then:

        DOMPaintTimingInfo paint_timing_info{
            .paint_time = performance->MonotonicTimeToDOMHighResTimeStamp(
                record.rendering_update_end_time),
            .presentation_time =
                performance->MonotonicTimeToDOMHighResTimeStamp(
                    frame_timing_details.presentation_feedback.timestamp)};

        // 12.3.1 Coarsen paintTimingInfo’s implementation-defined presentation
        // time to the next multiple of 4 milliseconds, or coarser.
        bool coarsen = !performance->CrossOriginIsolatedCapability();
        if (coarsen) {
          paint_timing_info.presentation_time =
              (frame_timing_details.presentation_feedback.timestamp -
               performance->GetTimeOriginInternal())
                  .CeilToMultiple(base::Milliseconds(4))
                  .InMillisecondsF();
        }

        auto flush = blink::BindOnce(
            std::move(flush_paint_timings),
            frame_timing_details.presentation_feedback.timestamp,
            paint_timing_info);

        if (coarsen) {
          // 12.3.2 Wait until the current high resolution time is
          // paintTimingInfo’s implementation-defined presentation time.
          // 12.4 Queue a global task on the performance timeline task source
          // given document’s relevant global object to run flushPaintTimings.
          base::TimeTicks target_time =
              performance->GetTimeOriginInternal() +
              base::Milliseconds(paint_timing_info.presentation_time);

          performance->GetTaskRunner().PostDelayedTask(
              FROM_HERE, std::move(flush),
              target_time - base::TimeTicks::Now());
        } else {
          std::move(flush).Run();
        }
      },
      WrapWeakPersistent(this), std::move(flush_paint_timings),
      paint_timing_record));
}

void PaintTiming::SetTickClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void PaintTiming::Trace(Visitor* visitor) const {
  visitor->Trace(fmp_detector_);
  Supplement<Document>::Trace(visitor);
}

PaintTiming::PaintTiming(Document& document)
    : Supplement<Document>(document),
      fmp_detector_(MakeGarbageCollected<FirstMeaningfulPaintDetector>(this)),
      clock_(base::DefaultTickClock::GetInstance()) {}

LocalFrame* PaintTiming::GetFrame() const {
  return GetSupplementable()->GetFrame();
}

void PaintTiming::NotifyPaintTimingChanged() {
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void PaintTiming::SetFirstPaint(base::TimeTicks stamp) {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  if (!relevant_paint_details.first_paint_.is_null()) {
    return;
  }

  DCHECK_EQ(IgnorePaintTimingScope::IgnoreDepth(), 0);

  relevant_paint_details.first_paint_ = stamp;

    LocalFrame* frame = GetFrame();
    if (frame && frame->GetDocument()) {
      frame->GetDocument()->MarkFirstPaint();
    }

  pending_paint_events_.insert(PaintEvent::kFirstPaint);
}

void PaintTiming::SetFirstContentfulPaint(base::TimeTicks stamp) {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  if (!relevant_paint_details.first_contentful_paint_.is_null()) {
    return;
  }
  DCHECK_EQ(IgnorePaintTimingScope::IgnoreDepth(), 0);

  relevant_paint_details.first_contentful_paint_ = stamp;

  // This only happens in hard navigations.
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  frame->View()->OnFirstContentfulPaint();

  if (frame->IsMainFrame() && frame->GetFrameScheduler()) {
    frame->GetFrameScheduler()->OnFirstContentfulPaintInMainFrame();
  }
  SetFirstPaint(stamp);
  Mark(PaintEvent::kFirstContentfulPaint);
  NotifyPaintTimingChanged();
}

void PaintTiming::Mark(PaintEvent event) {
  pending_paint_events_.insert(event);
}

void PaintTiming::
    RegisterNotifyFirstPaintAfterBackForwardCacheRestorePresentationTime(
        wtf_size_t index) {
  RegisterNotifyPresentationTime(
      BindOnce(&PaintTiming::
                   ReportFirstPaintAfterBackForwardCacheRestorePresentationTime,
               WrapWeakPersistent(this), index));
}

void PaintTiming::RegisterNotifyPresentationTime(ReportTimeCallback callback) {
  // ReportPresentationTime will queue a presentation-promise, the callback is
  // called when the compositor submission of the current render frame completes
  // or fails to happen.
  if (!GetFrame() || !GetFrame()->GetPage())
    return;
  GetFrame()->GetPage()->GetChromeClient().NotifyPresentationTime(
      *GetFrame(), std::move(callback));
}

void PaintTiming::ReportPresentationTime(
    PaintEvent event,
    base::TimeTicks rendering_update_end_time,
    const viz::FrameTimingDetails& presentation_details) {
  CHECK(IsMainThread());
  base::TimeTicks timestamp =
      presentation_details.presentation_feedback.timestamp;

  switch (event) {
    case PaintEvent::kFirstPaint:
      SetFirstPaintPresentation(
          PaintTimingInfo{rendering_update_end_time, timestamp});
      return;
    case PaintEvent::kFirstContentfulPaint:
      SetFirstContentfulPaintPresentation(
          PaintTimingInfo{rendering_update_end_time, timestamp});
      RecordFirstContentfulPaintTimingMetrics(presentation_details);
      return;
    case PaintEvent::kFirstImagePaint:
      SetFirstImagePaintPresentation(timestamp);
      return;
    default:
      NOTREACHED();
  }
}

void PaintTiming::RecordFirstContentfulPaintTimingMetrics(
    const viz::FrameTimingDetails& frame_timing_details) {
  if (frame_timing_details.received_compositor_frame_timestamp ==
          base::TimeTicks() ||
      frame_timing_details.embedded_frame_timestamp == base::TimeTicks()) {
    return;
  }
  bool frame_submitted_before_embed =
      (frame_timing_details.received_compositor_frame_timestamp <
       frame_timing_details.embedded_frame_timestamp);
  base::UmaHistogramBoolean("Navigation.FCPFrameSubmittedBeforeSurfaceEmbed",
                            frame_submitted_before_embed);

  if (frame_submitted_before_embed) {
    base::UmaHistogramCustomTimes(
        "Navigation.FCPFrameSubmissionToSurfaceEmbed",
        frame_timing_details.embedded_frame_timestamp -
            frame_timing_details.received_compositor_frame_timestamp,
        base::Milliseconds(1), base::Minutes(3), 50);
  } else {
    base::UmaHistogramCustomTimes(
        "Navigation.SurfaceEmbedToFCPFrameSubmission",
        frame_timing_details.received_compositor_frame_timestamp -
            frame_timing_details.embedded_frame_timestamp,
        base::Milliseconds(1), base::Minutes(3), 50);
  }
}

void PaintTiming::ReportFirstPaintAfterBackForwardCacheRestorePresentationTime(
    wtf_size_t index,
    const viz::FrameTimingDetails& presentation_details) {
  CHECK(IsMainThread());
  SetFirstPaintAfterBackForwardCacheRestorePresentation(
      presentation_details.presentation_feedback.timestamp, index);
}

void PaintTiming::SetFirstPaintPresentation(
    const PaintTimingInfo& paint_timing_info) {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  DCHECK(relevant_paint_details.first_paint_presentation_.is_null());
  relevant_paint_details.first_paint_presentation_ =
      paint_timing_info.presentation_time;
  if (first_paint_presentation_for_ukm_.is_null()) {
    first_paint_presentation_for_ukm_ = paint_timing_info.presentation_time;
  }
  probe::PaintTiming(
      GetSupplementable(), "firstPaint",
      relevant_paint_details.first_paint_presentation_.since_origin()
          .InSecondsF());
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstContentfulPaintPresentation(
    const PaintTimingInfo& paint_timing_info) {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  DCHECK(relevant_paint_details.first_contentful_paint_presentation_.is_null());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "benchmark,loading", "GlobalFirstContentfulPaint",
      TRACE_EVENT_SCOPE_GLOBAL, paint_timing_info.presentation_time);
  relevant_paint_details.first_contentful_paint_presentation_ =
      paint_timing_info.presentation_time;
  CHECK(first_contentful_paint_presentation_.is_null());
  first_contentful_paint_presentation_ = paint_timing_info.presentation_time;
  probe::PaintTiming(
      GetSupplementable(), "firstContentfulPaint",
      relevant_paint_details.first_contentful_paint_presentation_.since_origin()
          .InSecondsF());

  NotifyPaintTimingChanged();
  fmp_detector_->NotifyFirstContentfulPaint(
      paint_details_.first_contentful_paint_presentation_);
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*GetSupplementable());
  if (interactive_detector) {
    interactive_detector->OnFirstContentfulPaint(
        paint_details_.first_contentful_paint_presentation_);
  }

  WindowPerformance* performance = GetPerformanceInstance(GetFrame());
  if (GetFrame()) {
    PerformanceTimingForReporting* timing_for_reporting =
        performance->timingForReporting();
    GetFrame()->OnFirstContentfulPaint(
        paint_timing_info.presentation_time,
        timing_for_reporting->NavigationStartAsMonotonicTime());
    GetFrame()->Loader().Progress().DidFirstContentfulPaint();

    auto* coordinator = GetSupplementable()->GetResourceCoordinator();
    if (coordinator && GetFrame()->IsOutermostMainFrame()) {
      coordinator->OnFirstContentfulPaint(
          paint_timing_info.presentation_time -
          timing_for_reporting->NavigationStartAsMonotonicTime());
    }
  }
}

void PaintTiming::SetFirstImagePaintPresentation(base::TimeTicks stamp) {
  PaintDetails& relevant_paint_details = GetRelevantPaintDetails();
  DCHECK(relevant_paint_details.first_image_paint_presentation_.is_null());
  relevant_paint_details.first_image_paint_presentation_ = stamp;
  probe::PaintTiming(
      GetSupplementable(), "firstImagePaint",
      relevant_paint_details.first_image_paint_presentation_.since_origin()
          .InSecondsF());
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstPaintAfterBackForwardCacheRestorePresentation(
    base::TimeTicks stamp,
    wtf_size_t index) {
  // The elements are allocated when the page is restored from the cache.
  DCHECK_GE(first_paints_after_back_forward_cache_restore_presentation_.size(),
            index);
  DCHECK(first_paints_after_back_forward_cache_restore_presentation_[index]
             .is_null());
  first_paints_after_back_forward_cache_restore_presentation_[index] = stamp;
  NotifyPaintTimingChanged();
}

void PaintTiming::SetRequestAnimationFrameAfterBackForwardCacheRestore(
    wtf_size_t index,
    size_t count) {
  auto now = clock_->NowTicks();

  // The elements are allocated when the page is restored from the cache.
  DCHECK_LT(index,
            request_animation_frames_after_back_forward_cache_restore_.size());
  auto& current_rafs =
      request_animation_frames_after_back_forward_cache_restore_[index];
  DCHECK_LT(count, current_rafs.size());
  DCHECK_EQ(current_rafs[count], base::TimeTicks());
  current_rafs[count] = now;
}

void PaintTiming::OnRestoredFromBackForwardCache() {
  // Allocate the last element with 0, which indicates that the first paint
  // after this navigation doesn't happen yet.
  wtf_size_t index =
      first_paints_after_back_forward_cache_restore_presentation_.size();
  DCHECK_EQ(index,
            request_animation_frames_after_back_forward_cache_restore_.size());

  first_paints_after_back_forward_cache_restore_presentation_.push_back(
      base::TimeTicks());
  RegisterNotifyFirstPaintAfterBackForwardCacheRestorePresentationTime(index);

  request_animation_frames_after_back_forward_cache_restore_.push_back(
      RequestAnimationFrameTimesAfterBackForwardCacheRestore{});

  LocalFrame* frame = GetFrame();
  if (!frame->IsOutermostMainFrame()) {
    return;
  }

  Document* document = frame->GetDocument();
  DCHECK(document);

  // Cancel if there is already a registered callback.
  if (raf_after_bfcache_restore_measurement_callback_id_) {
    document->CancelAnimationFrame(
        raf_after_bfcache_restore_measurement_callback_id_);
    raf_after_bfcache_restore_measurement_callback_id_ = 0;
  }

  raf_after_bfcache_restore_measurement_callback_id_ =
      document->RequestAnimationFrame(
          MakeGarbageCollected<
              RecodingTimeAfterBackForwardCacheRestoreFrameCallback>(this,
                                                                     index));
}

}  // namespace blink
