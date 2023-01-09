// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/speed_limit_uma_listener.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/power_monitor/power_observer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"

namespace blink {

constexpr base::TimeDelta SpeedLimitUmaListener::kStatsReportingPeriod;

SpeedLimitUmaListener::SpeedLimitUmaListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      current_speed_limit_(mojom::blink::kSpeedLimitMax),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_);
  ScheduleReport();
}

SpeedLimitUmaListener::~SpeedLimitUmaListener() {
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.ThermalThrottlingEpisodes",
                           num_throttling_episodes_);
}

void SpeedLimitUmaListener::OnSpeedLimitChange(int32_t speed_limit) {
  base::AutoLock crit(lock_);
  if (current_speed_limit_ == mojom::blink::kSpeedLimitMax &&
      speed_limit < mojom::blink::kSpeedLimitMax)
    num_throttling_episodes_++;
  current_speed_limit_ = speed_limit;
}

void SpeedLimitUmaListener::ScheduleReport() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpeedLimitUmaListener::ReportStats,
                     weak_ptr_factory_.GetWeakPtr()),
      kStatsReportingPeriod);
}

void SpeedLimitUmaListener::ReportStats() {
  {
    base::AutoLock crit(lock_);
    UMA_HISTOGRAM_BOOLEAN(
        "WebRTC.PeerConnection.ThermalThrottling",
        current_speed_limit_ >= 0 &&
            current_speed_limit_ < mojom::blink::kSpeedLimitMax);
    if (current_speed_limit_ != mojom::blink::kSpeedLimitMax) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "WebRTC.PeerConnection.SpeedLimit", current_speed_limit_, 0,
          mojom::blink::kSpeedLimitMax - 1, mojom::blink::kSpeedLimitMax - 1);
    }
  }
  ScheduleReport();
}

}  // namespace blink
