// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

namespace {

using TabSwitchResult = ContentToVisibleTimeReporter::TabSwitchResult;

const char* GetHistogramSuffix(
    bool has_saved_frames,
    const mojom::RecordContentToVisibleTimeRequest& start_state) {
  if (has_saved_frames)
    return "WithSavedFrames";

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
  bool category_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("latency", &category_enabled);
  return category_enabled;
}

void RecordTabSwitchTraceEvent(base::TimeTicks start_time,
                               base::TimeTicks end_time,
                               TabSwitchResult result,
                               bool has_saved_frames,
                               bool destination_is_loaded) {
  if (!IsLatencyTraceCategoryEnabled()) {
    return;
  }

  using TabSwitchMeasurement = perfetto::protos::pbzero::TabSwitchMeasurement;
  DCHECK_GE(end_time, start_time);
  const auto track = perfetto::Track(base::trace_event::GetNextGlobalTraceId());
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
        if (has_saved_frames) {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_WITH_SAVED_FRAMES);
        } else if (destination_is_loaded) {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_LOADED_NO_SAVED_FRAMES);
        } else {
          measurement->set_tab_state(
              TabSwitchMeasurement::STATE_NOT_LOADED_NO_SAVED_FRAMES);
        }
      });
  TRACE_EVENT_END("latency", track, end_time);
}

// Records histogram and trace event for the unfolding latency.
void RecordUnfoldHistogramAndTraceEvent(
    base::TimeTicks begin_timestamp,
    const viz::FrameTimingDetails& frame_timing_details) {
  base::TimeTicks presentation_timestamp =
      frame_timing_details.presentation_feedback.timestamp;
  DCHECK((begin_timestamp != base::TimeTicks()));
  if (IsLatencyTraceCategoryEnabled()) {
    const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                                perfetto::ProcessTrack::Current());
    TRACE_EVENT_BEGIN("latency", "Unfold.Latency", track, begin_timestamp);
    TRACE_EVENT_END("latency", track, presentation_timestamp);
  }

  // Record the latency histogram.
  base::UmaHistogramTimes("Android.UnfoldToTablet.Latency2",
                          (presentation_timestamp - begin_timestamp));
}

}  // namespace

ContentToVisibleTimeReporter::ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::~ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::SuccessfulPresentationTimeCallback
ContentToVisibleTimeReporter::TabWasShown(
    bool has_saved_frames,
    mojom::RecordContentToVisibleTimeRequestPtr start_state) {
  DCHECK(!start_state->event_start_time.is_null());
  if (tab_switch_start_state_ &&
      tab_switch_start_state_->show_reason_tab_switching &&
      start_state->show_reason_tab_switching) {
    // Missed a tab hide, so record an incomplete tab switch. As a side effect
    // this will reset the state.
    //
    // This can happen when the tab is backgrounded, but still visible in a
    // visible capturer or VR, so the widget is never notified to hide.
    // TabWasHidden is only called correctly for *hidden* capturers (such as
    // picture-in-picture). See WebContentsImpl::CalculatePageVisibilityState
    // for more details.
    //
    // TODO(crbug.com/1289266): Refactor visibility states to call TabWasHidden
    // every time a tab is backgrounded, even if the content is still visible.
    RecordHistogramsAndTraceEvents(
        TabSwitchResult::kMissedTabHide, /*show_reason_tab_switching=*/true,
        /*show_reason_bfcache_restore=*/false, base::TimeTicks::Now());
  }
  // Note: Usually `tab_switch_start_state_` should be null here, but sometimes
  // it isn't (in practice, this happens on Mac - see crbug.com/1284500). This
  // can happen if TabWasShown() gets called twice without TabWasHidden() in
  // between (which is supposed to be impossible).
  // DCHECK(!tab_switch_start_state_);

  OverwriteTabSwitchStartState(std::move(start_state), has_saved_frames);

  // |tab_switch_start_state_| is only reset by RecordHistogramsAndTraceEvents
  // once the metrics have been emitted.
  return base::BindOnce(
      &ContentToVisibleTimeReporter::
          RecordHistogramsAndTraceEventsWithFrameTimingDetails,
      weak_ptr_factory_.GetWeakPtr(), TabSwitchResult::kSuccess,
      tab_switch_start_state_->show_reason_tab_switching,
      tab_switch_start_state_->show_reason_bfcache_restore);
}

ContentToVisibleTimeReporter::SuccessfulPresentationTimeCallback
ContentToVisibleTimeReporter::TabWasShown(bool has_saved_frames,
                                          base::TimeTicks event_start_time,
                                          bool destination_is_loaded,
                                          bool show_reason_tab_switching,
                                          bool show_reason_bfcache_restore) {
  return TabWasShown(
      has_saved_frames,
      mojom::RecordContentToVisibleTimeRequest::New(
          event_start_time, destination_is_loaded, show_reason_tab_switching,
          show_reason_bfcache_restore, /*show_reason_unfold=*/false));
}

ContentToVisibleTimeReporter::SuccessfulPresentationTimeCallback
ContentToVisibleTimeReporter::GetCallbackForNextFrameAfterUnfold(
    base::TimeTicks begin_timestamp) {
  return base::BindOnce(&RecordUnfoldHistogramAndTraceEvent, begin_timestamp);
}

void ContentToVisibleTimeReporter::TabWasHidden() {
  if (tab_switch_start_state_ &&
      tab_switch_start_state_->show_reason_tab_switching) {
    RecordHistogramsAndTraceEvents(TabSwitchResult::kIncomplete,
                                   /*show_reason_tab_switching=*/true,
                                   /*show_reason_bfcache_restore=*/false,
                                   base::TimeTicks::Now());
  }

  // No matter what the show reason, clear `tab_switch_start_state_` which is no
  // longer valid.
  ResetTabSwitchStartState();
}

void ContentToVisibleTimeReporter::
    RecordHistogramsAndTraceEventsWithFrameTimingDetails(
        TabSwitchResult tab_switch_result,
        bool show_reason_tab_switching,
        bool show_reason_bfcache_restore,
        const viz::FrameTimingDetails& frame_timing_details) {
  RecordHistogramsAndTraceEvents(
      tab_switch_result, show_reason_tab_switching, show_reason_bfcache_restore,
      frame_timing_details.presentation_feedback.timestamp);
}

void ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents(
    TabSwitchResult tab_switch_result,
    bool show_reason_tab_switching,
    bool show_reason_bfcache_restore,
    base::TimeTicks presentation_timestamp) {
  DCHECK(tab_switch_start_state_);
  // If the DCHECK fail, make sure RenderWidgetHostImpl::WasShown was triggered
  // for recording the event.
  DCHECK(show_reason_bfcache_restore || show_reason_tab_switching);

  // Make sure to reset tab switch information when this function returns.
  absl::Cleanup reset_state = [this] { ResetTabSwitchStartState(); };

  if (show_reason_bfcache_restore) {
    RecordBackForwardCacheRestoreMetric(
        tab_switch_start_state_->event_start_time, presentation_timestamp);
  }

  if (!show_reason_tab_switching) {
    return;
  }

  RecordTabSwitchTraceEvent(tab_switch_start_state_->event_start_time,
                            presentation_timestamp, tab_switch_result,
                            has_saved_frames_,
                            tab_switch_start_state_->destination_is_loaded);

  const auto tab_switch_duration =
      presentation_timestamp - tab_switch_start_state_->event_start_time;

  const char* suffix =
      GetHistogramSuffix(has_saved_frames_, *tab_switch_start_state_);

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

void ContentToVisibleTimeReporter::OverwriteTabSwitchStartState(
    mojom::RecordContentToVisibleTimeRequestPtr state,
    bool has_saved_frames) {
  if (tab_switch_start_state_) {
    // Invalidate previously issued callbacks, to avoid accessing
    // `tab_switch_start_state_` which is about to be deleted.
    //
    // TODO(crbug.com/1289266): Make sure that TabWasShown() is never called
    // twice without a call to TabWasHidden() in-between, and remove this
    // mitigation.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
  tab_switch_start_state_ = std::move(state);
  has_saved_frames_ = has_saved_frames;
}

}  // namespace blink
