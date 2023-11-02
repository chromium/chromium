// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/zombie_host_detector.h"

#include "base/check.h"
#include "base/logging.h"
#include "net/base/network_change_notifier.h"

namespace {

bool IsMaxDurationExceeded(base::TimeTicks event_time,
                           base::TimeDelta max_duration,
                           const std::string& event_name) {
  if (event_time.is_null()) {
    VLOG(1) << "Last " << event_name << " is null.";
    return false;
  }
  base::TimeDelta event_duration = base::TimeTicks::Now() - event_time;
  DCHECK_GE(event_duration, base::TimeDelta()) << "Event duration is negative.";
  bool is_exceeded = event_duration > max_duration;
  if (is_exceeded) {
    LOG(ERROR) << "Last " << event_name << " happened " << event_duration
               << " ago, exceeding max duration of " << max_duration;
  } else {
    VLOG(1) << "Last " << event_name << " happened " << event_duration
            << " ago.";
  }
  return is_exceeded;
}

}  // namespace

namespace remoting {

// static
constexpr base::TimeDelta ZombieHostDetector::kZombieStateDetectionInterval;
constexpr base::TimeDelta ZombieHostDetector::kMaxHeartbeatInterval;
constexpr base::TimeDelta ZombieHostDetector::kMaxSignalingActiveInterval;

ZombieHostDetector::ZombieHostDetector(
    base::OnceClosure on_zombie_state_detected) {
  DCHECK(on_zombie_state_detected);

  on_zombie_state_detected_ = std::move(on_zombie_state_detected);
}

ZombieHostDetector::~ZombieHostDetector() = default;

void ZombieHostDetector::Start() {
  check_zombie_state_timer_.Start(FROM_HERE, kZombieStateDetectionInterval,
                                  this, &ZombieHostDetector::CheckZombieState);
}

void ZombieHostDetector::OnHeartbeatSent() {
  last_heartbeat_time_ = base::TimeTicks::Now();
}

void ZombieHostDetector::OnSignalingActive() {
  last_signaling_active_time_ = base::TimeTicks::Now();
}

base::TimeTicks ZombieHostDetector::GetNextDetectionTime() const {
  return check_zombie_state_timer_.desired_run_time();
}

void ZombieHostDetector::CheckZombieState() {
  VLOG(1) << "Detecting zombie state...";

  if (net::NetworkChangeNotifier::GetConnectionType() ==
      net::NetworkChangeNotifier::CONNECTION_NONE) {
    // The host shouldn't be considered in zombie state if it has no connection.
    VLOG(1) << "No internet connectivity. Skipping zombie state check...";
    previously_offline_ = true;
    return;
  }

  if (previously_offline_) {
    // If the host was previously offline, heartbeat/signaling might not have
    // happened at this point due to backoff. Reset them to |now| to allow them
    // to come through.

    previously_offline_ = false;

    VLOG(1) << "Host is going online. Previous heartbeat time: "
            << last_heartbeat_time_ << ", previous signaling active time: "
            << last_signaling_active_time_ << ". These will be reset to |now|.";

    // Don't reset if they are null, which happens when the first
    // heartbeat/signaling attempt has not succeeded yet.
    if (!last_heartbeat_time_.is_null()) {
      OnHeartbeatSent();
    }
    if (!last_signaling_active_time_.is_null()) {
      OnSignalingActive();
    }
    return;
  }

  if (IsMaxDurationExceeded(last_heartbeat_time_, kMaxHeartbeatInterval,
                            "heartbeat") ||
      IsMaxDurationExceeded(last_signaling_active_time_,
                            kMaxSignalingActiveInterval,
                            "signaling activity")) {
    LOG(ERROR) << "Host zombie state detected.";
    check_zombie_state_timer_.Stop();
    std::move(on_zombie_state_detected_).Run();
    return;
  }

  VLOG(1) << "No zombie state detected.";
}

}  // namespace remoting
