// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/tab_switch_time_recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

namespace {

//  Used to generate unique "TabSwitching::Latency" event ids. Note: The address
//  of TabSwitchTimeRecorder can't be used as an id because a single
//  TabSwitchTimeRecorder can generate multiple overlapping events.
int g_num_trace_events_in_process = 0;

const char* GetHistogramSuffix(bool has_saved_frames,
                               const RecordTabSwitchTimeRequest& start_state) {
  if (has_saved_frames)
    return "WithSavedFrames";

  if (start_state.destination_is_loaded) {
    if (start_state.destination_is_frozen) {
      return "NoSavedFrames_Loaded_Frozen";
    } else {
      return "NoSavedFrames_Loaded_NotFrozen";
    }
  } else {
    return "NoSavedFrames_NotLoaded";
  }
}

}  // namespace

RecordTabSwitchTimeRequest::RecordTabSwitchTimeRequest(
    base::TimeTicks tab_switch_start_time,
    bool destination_is_loaded,
    bool destination_is_frozen)
    : tab_switch_start_time(tab_switch_start_time),
      destination_is_loaded(destination_is_loaded),
      destination_is_frozen(destination_is_frozen) {}

TabSwitchTimeRecorder::TabSwitchTimeRecorder() {}

TabSwitchTimeRecorder::~TabSwitchTimeRecorder() {}

base::OnceCallback<void(const gfx::PresentationFeedback&)>
TabSwitchTimeRecorder::TabWasShown(
    bool has_saved_frames,
    const RecordTabSwitchTimeRequest& start_state,
    base::TimeTicks render_widget_visibility_request_timestamp) {
  DCHECK(!start_state.tab_switch_start_time.is_null());
  DCHECK(!render_widget_visibility_request_timestamp.is_null());
  DCHECK(!tab_switch_start_state_);
  DCHECK(render_widget_visibility_request_timestamp_.is_null());

  if (tab_switch_start_state_) {
    // TabWasShown() is called multiple times without the tab being hidden in
    // between. This shouldn't happen per the DCHECK above. Dump without
    // crashing to gather more information for https://crbug.com/981757.
    //
    // TODO(fdoray): This code should be removed no later than August 30, 2019.
    // https://crbug.com/981757
    base::debug::DumpWithoutCrashing();
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  has_saved_frames_ = has_saved_frames;
  tab_switch_start_state_ = start_state;
  render_widget_visibility_request_timestamp_ =
      render_widget_visibility_request_timestamp;

  // |tab_switch_start_state_| is only reset by RecordHistogramsAndTraceEvents
  // once the metrics have been emitted.
  return base::BindOnce(&TabSwitchTimeRecorder::RecordHistogramsAndTraceEvents,
                        weak_ptr_factory_.GetWeakPtr(),
                        false /* is_incomplete */);
}

void TabSwitchTimeRecorder::TabWasHidden() {
  if (tab_switch_start_state_) {
    RecordHistogramsAndTraceEvents(true /* is_incomplete */,
                                   gfx::PresentationFeedback::Failure());
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

void TabSwitchTimeRecorder::RecordHistogramsAndTraceEvents(
    bool is_incomplete,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(tab_switch_start_state_);
  DCHECK(!render_widget_visibility_request_timestamp_.is_null());

  auto tab_switch_result = TabSwitchResult::kSuccess;
  if (is_incomplete)
    tab_switch_result = TabSwitchResult::kIncomplete;
  else if (feedback.flags & gfx::PresentationFeedback::kFailure)
    tab_switch_result = TabSwitchResult::kPresentationFailure;

  const auto tab_switch_duration =
      feedback.timestamp - tab_switch_start_state_->tab_switch_start_time;

  // Record trace events.
  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process),
      tab_switch_start_state_->tab_switch_start_time);
  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP2(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process), feedback.timestamp,
      "result", tab_switch_result, "latency",
      tab_switch_duration.InMillisecondsF());
  ++g_num_trace_events_in_process;

  // Each value recorded to a histogram with suffix .NoSavedFrames_Loaded_Frozen
  // or .NoSavedFrames_Loaded_NotFrozen is also recorded to a histogram with
  // suffix .NoSavedFrames_Loaded to facilitate assessing the impact of
  // experiments that affect the number of frozen tab on tab switch time.
  const bool is_no_saved_frames_loaded =
      !has_saved_frames_ &&
      tab_switch_start_state_.value().destination_is_loaded;

  // Record result histogram.
  base::UmaHistogramEnumeration(
      std::string("Browser.Tabs.TabSwitchResult.") +
          GetHistogramSuffix(has_saved_frames_,
                             tab_switch_start_state_.value()),
      tab_switch_result);

  if (is_no_saved_frames_loaded) {
    UMA_HISTOGRAM_ENUMERATION(
        "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded", tab_switch_result);
  }

  // Record latency histogram.
  switch (tab_switch_result) {
    case TabSwitchResult::kSuccess: {
      base::UmaHistogramTimes(
          std::string("Browser.Tabs.TotalSwitchDuration.") +
              GetHistogramSuffix(has_saved_frames_,
                                 tab_switch_start_state_.value()),
          tab_switch_duration);

      if (is_no_saved_frames_loaded) {
        UMA_HISTOGRAM_TIMES(
            "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded",
            tab_switch_duration);
      }
      break;
    }
    case TabSwitchResult::kIncomplete: {
      base::UmaHistogramTimes(
          std::string("Browser.Tabs.TotalIncompleteSwitchDuration.") +
              GetHistogramSuffix(has_saved_frames_,
                                 tab_switch_start_state_.value()),
          tab_switch_duration);

      if (is_no_saved_frames_loaded) {
        UMA_HISTOGRAM_TIMES(
            "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded",
            tab_switch_duration);
      }
      break;
    }
    case TabSwitchResult::kPresentationFailure: {
      break;
    }
  }

  // Record legacy latency histogram.
  UMA_HISTOGRAM_TIMES(
      "MPArch.RWH_TabSwitchPaintDuration",
      feedback.timestamp - render_widget_visibility_request_timestamp_);

  // Reset tab switch information.
  has_saved_frames_ = false;
  tab_switch_start_state_.reset();
  render_widget_visibility_request_timestamp_ = base::TimeTicks();
}

}  // namespace content
