// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
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

const char* GetHistogramSuffix(
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
  DCHECK_GE(end_time, start_time);
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

void RecordTabSwitchHistogramsAndTraceEvent(
    TabSwitchResult tab_switch_result,
    base::TimeTicks start_time,
    base::TimeTicks presentation_timestamp,
    const VisibleTimeEvent::TabSwitchReason& start_state) {
  uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
  RecordTabSwitchTraceEvent(start_time, presentation_timestamp,
                            tab_switch_result, start_state, event_id);

  const auto tab_switch_duration = presentation_timestamp - start_time;

  const char* suffix = GetHistogramSuffix(start_state);
  base::trace_event::HistogramScope scoped_event(event_id);

  // Record result histogram.
  base::UmaHistogramEnumeration("Browser.Tabs.TabSwitchResult3",
                                tab_switch_result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Browser.Tabs.TabSwitchResult3.", suffix}),
      tab_switch_result);

  // Record latency histogram.
  switch (tab_switch_result) {
    case TabSwitchResult::kSuccess:
      base::UmaHistogramMediumTimes("Browser.Tabs.TotalSwitchDuration3",
                                    tab_switch_duration);
      base::UmaHistogramMediumTimes(
          base::StrCat({"Browser.Tabs.TotalSwitchDuration3.", suffix}),
          tab_switch_duration);
      break;
    case TabSwitchResult::kMissedTabHide:
    case TabSwitchResult::kIncomplete:
      base::UmaHistogramMediumTimes(
          "Browser.Tabs.TotalIncompleteSwitchDuration3", tab_switch_duration);
      base::UmaHistogramMediumTimes(
          base::StrCat(
              {"Browser.Tabs.TotalIncompleteSwitchDuration3.", suffix}),
          tab_switch_duration);
      break;
  }
}

}  // namespace

ContentToVisibleTimeReporter::ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::~ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::SuccessfulPresentationTimeCallback
ContentToVisibleTimeReporter::TabWasShown(
    RecordContentToVisibleTimeRequest start_state) {
#if DCHECK_IS_ON()
  for (const auto& event : start_state.events) {
    DCHECK(!event.event_start_time.is_null());
  }
#endif

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
            std::get<VisibleTimeEvent::TabSwitchReason>(event.reason));
      }
    }
  }
  // Note: Usually `tab_switch_start_state_` should be null here, but sometimes
  // it isn't (in practice, this happens on Mac - see crbug.com/1284500). This
  // can happen if TabWasShown() gets called twice without TabWasHidden() in
  // between (which is supposed to be impossible).
  // DCHECK(tab_switch_start_state_.empty());

  OverwriteTabSwitchStartState(std::move(start_state));

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
            std::get<VisibleTimeEvent::TabSwitchReason>(event.reason));
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
  for (const VisibleTimeEvent& event : tab_switch_start_state_->events) {
    std::visit(absl::Overload{
                   [&](const VisibleTimeEvent::TabSwitchReason& tab_switch) {
                     RecordTabSwitchHistogramsAndTraceEvent(
                         tab_switch_result, event.event_start_time,
                         presentation_timestamp, tab_switch);
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
    std::optional<RecordContentToVisibleTimeRequest> state) {
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
}

}  // namespace blink
