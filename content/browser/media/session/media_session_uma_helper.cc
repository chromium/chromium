// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_uma_helper.h"

#include <utility>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"

namespace content {

using HistogramBase = base::HistogramBase;

MediaSessionUmaHelper::MediaSessionUmaHelper()
    : clock_(base::DefaultTickClock::GetInstance()) {}

MediaSessionUmaHelper::~MediaSessionUmaHelper()
{}

void MediaSessionUmaHelper::RecordEnterPictureInPicture(
    EnterPictureInPictureType type) const {
  base::UmaHistogramEnumeration("Media.Session.EnterPictureInPictureV2", type);
}

void MediaSessionUmaHelper::OnSessionActive() {
  current_active_time_ = clock_->NowTicks();
}

void MediaSessionUmaHelper::OnSessionSuspended() {
  if (current_active_time_.is_null())
    return;

  total_active_time_ += clock_->NowTicks() - current_active_time_;
  current_active_time_ = base::TimeTicks();
}

void MediaSessionUmaHelper::OnSessionInactive() {
  if (!current_active_time_.is_null()) {
    total_active_time_ += clock_->NowTicks() - current_active_time_;
    current_active_time_ = base::TimeTicks();
  }

  if (total_active_time_.is_zero())
    return;

  UMA_HISTOGRAM_LONG_TIMES("Media.Session.ActiveTime", total_active_time_);
  total_active_time_ = base::TimeDelta();
}

void MediaSessionUmaHelper::OnServiceDestroyed() {
  if (!total_pip_time_for_session_) {
    return;
  }

  UMA_HISTOGRAM_LONG_TIMES("Media.Session.PictureInPicture.TotalTimeForSession",
                           total_pip_time_for_session_.value());
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.Session.PictureInPicture.TotalTimeForSessionV2",
      total_pip_time_for_session_.value(), base::Milliseconds(1),
      base::Hours(10), 100);

  total_pip_time_for_session_ = std::nullopt;
}

void MediaSessionUmaHelper::OnMediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  if (is_picture_in_picture) {
    current_enter_pip_time_ = clock_->NowTicks();
    return;
  }

  if (!current_enter_pip_time_) {
    return;
  }

  const base::TimeDelta total_pip_time =
      clock_->NowTicks() - current_enter_pip_time_.value();
  current_enter_pip_time_ = std::nullopt;

  if (!total_pip_time_for_session_) {
    total_pip_time_for_session_ = total_pip_time;
  } else {
    total_pip_time_for_session_.value() += total_pip_time;
  }
}

void MediaSessionUmaHelper::SetClockForTest(
    const base::TickClock* testing_clock) {
  clock_ = testing_clock;
}

}  // namespace content
