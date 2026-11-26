// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/histogram_scope.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

namespace {

using TabSwitchResult = ContentToVisibleTimeReporter::TabSwitchResult;

const char* GetSavedFrameHistogramSuffix(
    const VisibleTimeEvent::TabSwitchReason& start_state) {
  if (start_state.had_saved_frame_at_start) {
    return "WithSavedFrames";
  }

  if (start_state.destination_is_loaded) {
    return "NoSavedFrames_Loaded";
  } else {
    return "NoSavedFrames_NotLoaded";
  }
}

const char* GetFrozenStateHistogramSuffix(
    const VisibleTimeEvent::TabSwitchReason& start_state) {
  if (!start_state.destination_is_loaded) {
    return "FrozenState.NotLoaded";
  }
  return start_state.destination_is_frozen ? "FrozenState.Loaded_Frozen"
                                           : "FrozenState.Loaded_Unfrozen";
}

void RecordTabSwitchHistogramVariants(
    std::string_view histogram_name,
    const VisibleTimeEvent::TabSwitchReason& start_state,
    base::FunctionRef<void(std::string_view)> record_histogram) {
  record_histogram(histogram_name);
  record_histogram(base::StrCat(
      {histogram_name, ".", GetSavedFrameHistogramSuffix(start_state)}));
  record_histogram(base::StrCat(
      {histogram_name, ".", GetFrozenStateHistogramSuffix(start_state)}));
}

void RecordBackForwardCacheRestoreMetric(
    const base::TimeTicks requested_time,
    base::TimeTicks presentation_timestamp) {
  const base::TimeDelta delta = presentation_timestamp - requested_time;
  // Histogram to record the content to visible duration after restoring a page
  // from back-forward cache. Here min, max bucket size are same as the
  // "PageLoad.PaintTiming.NavigationToFirstContentfulPaint" metric.
  base::UmaHistogramCustomTimes(
      "BackForwardCache.Restore.NavigationToFirstPaint", delta,
      base::Milliseconds(10), base::Minutes(10), 100);
}

bool IsLatencyTraceCategoryEnabled() {
  // Avoid unnecessary work to compute a track.
  return TRACE_EVENT_CATEGORY_ENABLED("latency");
}

void RecordTabSwitchTraceEvent(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    TabSwitchResult result,
    const VisibleTimeEvent::TabSwitchReason& start_state,
    uint64_t flow_id) {
  if (!IsLatencyTraceCategoryEnabled()) {
    return;
  }

  using TabSwitchMeasurement = perfetto::protos::pbzero::TabSwitchMeasurement;
  // Clamp because `start_time` and `end_time` come from different processes.
  end_time = std::max(end_time, start_time);
  const auto track =
      perfetto::Track::Global(base::trace_event::GetNextGlobalTraceId());
  const auto flow = perfetto::Flow::Global(flow_id);
  TRACE_EVENT_BEGIN(
      "latency", "TabSwitching::Latency", track, start_time,
      [&](perfetto::EventContext ctx) {
        TabSwitchMeasurement* measurement =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_tab_switch_measurement();
        switch (result) {
          case TabSwitchResult::kSuccess:
            measurement->set_result(TabSwitchMeasurement::RESULT_SUCCESS);
            break;
          case TabSwitchResult::kIncomplete:
            measurement->set_result(TabSwitchMeasurement::RESULT_INCOMPLETE);
            break;
          case TabSwitchResult::kMissedTabHide:
            measurement->set_result(
                TabSwitchMeasurement::RESULT_MISSED_TAB_HIDE);
            break;
        }
        if (start_state.had_saved_frame_at_start) {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_WITH_SAVED_FRAMES);
        } else if (start_state.destination_is_loaded) {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_LOADED_NO_SAVED_FRAMES);
        } else {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_NOT_LOADED_NO_SAVED_FRAMES);
        }
      });
  TRACE_EVENT_END("latency", track, end_time, flow);
}

// Intermediate timestamps of a tab switch presented by the renderer.
struct RendererTabSwitchTimestamps {
  // RecordContentToVisibleTimeRequest::browser_dispatch_time: when
  // content::RenderWidgetHostImpl sent the WasShown or
  // RequestSuccessfulPresentationTimeForNextFrame IPC to the renderer.
  base::TimeTicks browser_dispatch_time;
  // When the renderer main thread started handling that IPC.
  base::TimeTicks renderer_main_thread_start_time;
};

// Records the duration of each phase of a tab switch to a loaded tab without a
// saved frame. The phases add up to
// Browser.Tabs.TotalSwitchDuration3.NoSavedFrames_Loaded.
void RecordNoSavedFramesLoadedPhaseDurations(
    base::TimeTicks start_time,
    const RendererTabSwitchTimestamps& renderer_timestamps,
    base::TimeTicks presentation_timestamp) {
  const auto record_phase = [](std::string_view phase, base::TimeTicks begin,
                               base::TimeTicks end) {
    // Clamp because timestamps come from different processes.
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"Browser.Tabs.NoSavedFramesLoaded.SwitchPhaseDuration.", phase}),
        std::max(base::TimeDelta(), end - begin));
  };
  record_phase("BrowserMainThread", start_time,
               renderer_timestamps.browser_dispatch_time);
  record_phase("BrowserToRendererMainThread",
               renderer_timestamps.browser_dispatch_time,
               renderer_timestamps.renderer_main_thread_start_time);
  record_phase("RendererMainThreadToPresentation",
               renderer_timestamps.renderer_main_thread_start_time,
               presentation_timestamp);
}

void RecordTabSwitchHistogramsAndTraceEvent(
    TabSwitchResult tab_switch_result,
    base::TimeTicks start_time,
    base::TimeTicks presentation_timestamp,
    const VisibleTimeEvent::TabSwitchReason& start_state,
    const std::optional<RendererTabSwitchTimestamps>& renderer_timestamps) {
  uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
  RecordTabSwitchTraceEvent(start_time, presentation_timestamp,
                            tab_switch_result, start_state, event_id);

  const auto tab_switch_duration = presentation_timestamp - start_time;

  base::trace_event::HistogramScope scoped_event(event_id);

  // Record result histogram.
  RecordTabSwitchHistogramVariants(
      "Browser.Tabs.TabSwitchResult3", start_state,
      [tab_switch_result](std::string_view histogram_name) {
        base::UmaHistogramEnumeration(histogram_name, tab_switch_result);
      });

  // Record latency histogram.
  const auto record_duration_histogram =
      [tab_switch_duration](std::string_view histogram_name) {
        base::UmaHistogramMediumTimes(histogram_name, tab_switch_duration);
      };
  switch (tab_switch_result) {
    case TabSwitchResult::kSuccess:
      RecordTabSwitchHistogramVariants("Browser.Tabs.TotalSwitchDuration3",
                                       start_state, record_duration_histogram);
      if (renderer_timestamps && start_state.destination_is_loaded &&
          !start_state.had_saved_frame_at_start) {
        RecordNoSavedFramesLoadedPhaseDurations(
            start_time, *renderer_timestamps, presentation_timestamp);
      }
      break;
    case TabSwitchResult::kMissedTabHide:
    case TabSwitchResult::kIncomplete:
      RecordTabSwitchHistogramVariants(
          "Browser.Tabs.TotalIncompleteSwitchDuration3", start_state,
          record_duration_histogram);
      break;
  }
}

}  // namespace

ContentToVisibleTimeReporter::ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::~ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::SuccessfulPresentationTimeCallback
ContentToVisibleTimeReporter::TabWasShown(
    RecordContentToVisibleTimeRequest start_state,
    std::optional<base::TimeTicks> renderer_main_thread_start_time) {
#if DCHECK_IS_ON()
  for (const auto& event : start_state.events) {
    DCHECK(!event.event_start_time.is_null());
  }
#endif
  // Requests sent to the renderer by content::RenderWidgetHostImpl have a
  // `browser_dispatch_time`, and blink::WidgetBase provides
  // `renderer_main_thread_start_time` for them. Other requests have neither.
  CHECK_EQ(start_state.browser_dispatch_time.has_value(),
           renderer_main_thread_start_time.has_value());

  const bool has_tab_switch = std::ranges::any_of(
      start_state.events, [](const VisibleTimeEvent& event) {
        return std::holds_alternative<VisibleTimeEvent::TabSwitchReason>(
            event.reason);
      });
  const bool has_bfcache_restore = std::ranges::any_of(
      start_state.events, [](const VisibleTimeEvent& event) {
        return std::holds_alternative<VisibleTimeEvent::BFCacheRestoreReason>(
            event.reason);
      });
  base::UmaHistogramBoolean(
      "Browser.Tabs.TabShowReason.BothTabSwitchingAndBfcache",
      has_tab_switch && has_bfcache_restore);

  if (tab_switch_start_state_ && has_tab_switch) {
    for (const VisibleTimeEvent& event : tab_switch_start_state_->events) {
      if (std::holds_alternative<VisibleTimeEvent::TabSwitchReason>(
              event.reason)) {
        // Missed a tab hide, so record an incomplete tab switch before
        // resetting the state.
        //
        // This can happen when the tab is backgrounded, but still visible in a
        // visible capturer or VR, so the widget is never notified to hide.
        // TabWasHidden is only called correctly for *hidden* capturers (such as
        // picture-in-picture). See
        // WebContentsImpl::CalculatePageVisibilityState for more details.
        //
        // TODO(crbug.com/40211849): Refactor visibility states to call
        // TabWasHidden every time a tab is backgrounded, even if the content is
        // still visible.
        RecordTabSwitchHistogramsAndTraceEvent(
            TabSwitchResult::kMissedTabHide, event.event_start_time,
            base::TimeTicks::Now(),
            std::get<VisibleTimeEvent::TabSwitchReason>(event.reason),
            /*renderer_timestamps=*/std::nullopt);
      }
    }
  }
  // Note: Usually `tab_switch_start_state_` should be null here, but sometimes
  // it isn't (in practice, this happens on Mac - see crbug.com/1284500). This
  // can happen if TabWasShown() gets called twice without TabWasHidden() in
  // between (which is supposed to be impossible).
  // DCHECK(tab_switch_start_state_.empty());

  OverwriteTabSwitchStartState(std::move(start_state),
                               renderer_main_thread_start_time);

  // |tab_switch_start_state_| is only reset by ResetTabSwitchStartState once
  // the metrics have been emitted.
  return base::BindOnce(
      &ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents,
      weak_ptr_factory_.GetWeakPtr(), TabSwitchResult::kSuccess);
}

void ContentToVisibleTimeReporter::TabWasHidden() {
  if (tab_switch_start_state_) {
    for (const VisibleTimeEvent& event : tab_switch_start_state_->events) {
      if (std::holds_alternative<VisibleTimeEvent::TabSwitchReason>(
              event.reason)) {
        RecordTabSwitchHistogramsAndTraceEvent(
            TabSwitchResult::kIncomplete, event.event_start_time,
            base::TimeTicks::Now(),
            std::get<VisibleTimeEvent::TabSwitchReason>(event.reason),
            /*renderer_timestamps=*/std::nullopt);
      }
    }
  }
  // No matter what the show reason, clear `tab_switch_start_state_` which is no
  // longer valid.
  ResetTabSwitchStartState();
}

void ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents(
    TabSwitchResult tab_switch_result,
    const viz::FrameTimingDetails& frame_timing_details) {
  const base::TimeTicks presentation_timestamp =
      frame_timing_details.presentation_feedback.timestamp;

  DCHECK(tab_switch_start_state_);
  // If the DCHECK fail, make sure RenderWidgetHostImpl::WasShown was triggered
  // for recording the event.

  // Phase durations compare timestamps from the browser and renderer
  // processes, which is only meaningful if they use a consistent clock.
  std::optional<RendererTabSwitchTimestamps> renderer_timestamps;
  if (renderer_main_thread_start_time_ &&
      base::TimeTicks::IsConsistentAcrossProcesses()) {
    renderer_timestamps = RendererTabSwitchTimestamps{
        .browser_dispatch_time =
            *tab_switch_start_state_->browser_dispatch_time,
        .renderer_main_thread_start_time = *renderer_main_thread_start_time_};
  }
  for (const VisibleTimeEvent& event : tab_switch_start_state_->events) {
    std::visit(absl::Overload{
                   [&](const VisibleTimeEvent::TabSwitchReason& tab_switch) {
                     RecordTabSwitchHistogramsAndTraceEvent(
                         tab_switch_result, event.event_start_time,
                         presentation_timestamp, tab_switch,
                         renderer_timestamps);
                   },
                   [&](const VisibleTimeEvent::BFCacheRestoreReason&) {
                     RecordBackForwardCacheRestoreMetric(
                         event.event_start_time, presentation_timestamp);
                   },
               },
               event.reason);
  }

  ResetTabSwitchStartState();
}

void ContentToVisibleTimeReporter::OverwriteTabSwitchStartState(
    std::optional<RecordContentToVisibleTimeRequest> state,
    std::optional<base::TimeTicks> renderer_main_thread_start_time) {
  if (tab_switch_start_state_) {
    // Invalidate previously issued callbacks, to avoid accessing
    // `tab_switch_start_state_` which is about to be deleted.
    //
    // TODO(crbug.com/40211849): Make sure that TabWasShown() is never called
    // twice without a call to TabWasHidden() in-between, and remove this
    // mitigation.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
  tab_switch_start_state_ = std::move(state);
  renderer_main_thread_start_time_ = renderer_main_thread_start_time;
}

}  // namespace blink
