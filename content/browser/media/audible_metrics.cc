// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audible_metrics.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"

namespace content {

AudibleMetrics::AudibleMetrics()
    : max_concurrent_audible_web_contents_in_session_(0),
      clock_(base::DefaultTickClock::GetInstance()) {}

AudibleMetrics::~AudibleMetrics() = default;

void AudibleMetrics::UpdateAudibleWebContentsState(
    const WebContents* web_contents, bool audible) {
  bool found =
      audible_web_contents_.find(web_contents) != audible_web_contents_.end();
  if (found == audible)
    return;

  if (audible)
    AddAudibleWebContents(web_contents);
  else
    RemoveAudibleWebContents(web_contents);
}

void AudibleMetrics::WebContentsDestroyed(const WebContents* web_contents,
                                          bool recently_audible) {
  if (base::Contains(audible_web_contents_, web_contents))
    RemoveAudibleWebContents(web_contents);

  // If we have two web contents and we go down to one, we should record
  // whether we destroyed the most recent one. This is used to determine
  // whether a user closes a new or old tab after starting playback if
  // they have multiple tabs.
  if (audible_web_contents_.size() == 1 && recently_audible) {
    ExitConcurrentPlaybackContents value =
        last_audible_web_contents_.back() == web_contents
            ? ExitConcurrentPlaybackContents::kMostRecent
            : ExitConcurrentPlaybackContents::kOlder;

    UMA_HISTOGRAM_ENUMERATION(
        "Media.Audible.CloseNewestToExitConcurrentPlayback", value);
  }

  last_audible_web_contents_.remove(web_contents);
}

void AudibleMetrics::SetClockForTest(const base::TickClock* test_clock) {
  clock_ = test_clock;
}

void AudibleMetrics::AddAudibleWebContents(const WebContents* web_contents) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Media.Audible.ConcurrentTabsWhenStarting", audible_web_contents_.size(),
      1, 10, 11);

  audible_web_contents_.insert(web_contents);

  // Since the web contents is newly audible then move it to the back of the
  // last audible web contents list.
  last_audible_web_contents_.remove(web_contents);
  last_audible_web_contents_.push_back(web_contents);

  if (audible_web_contents_.size() > 1 &&
      concurrent_web_contents_start_time_.is_null()) {
    concurrent_web_contents_start_time_ = clock_->NowTicks();
  }

  if (audible_web_contents_.size() >
      max_concurrent_audible_web_contents_in_session_) {
    max_concurrent_audible_web_contents_in_session_ =
        audible_web_contents_.size();

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.Audible.MaxConcurrentTabsInSession",
        max_concurrent_audible_web_contents_in_session_,
        1, 10, 11);
  }
}

void AudibleMetrics::RemoveAudibleWebContents(const WebContents* web_contents) {
  audible_web_contents_.erase(web_contents);

  if (audible_web_contents_.size() <= 1 &&
      !concurrent_web_contents_start_time_.is_null()) {
    base::TimeDelta concurrent_total_time =
        clock_->NowTicks() - concurrent_web_contents_start_time_;
    concurrent_web_contents_start_time_ = base::TimeTicks();

    UMA_HISTOGRAM_LONG_TIMES("Media.Audible.ConcurrentTabsTime",
                             concurrent_total_time);
  }
}

}  // namespace content
