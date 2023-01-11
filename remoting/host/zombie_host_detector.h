// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ZOMBIE_HOST_DETECTOR_H_
#define REMOTING_HOST_ZOMBIE_HOST_DETECTOR_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/signaling/signaling_tracker.h"

namespace remoting {

// Helper class to detect if the host has entered a zombie state, where the host
// process is still running and has internet connection, but the host appears to
// be offline, or isn't reachable through signaling.
class ZombieHostDetector final : public HeartbeatSender::Observer,
                                 public SignalingTracker {
 public:
  static constexpr base::TimeDelta kZombieStateDetectionInterval =
      base::Minutes(5);
  static constexpr base::TimeDelta kMaxHeartbeatInterval = base::Minutes(15);
  static constexpr base::TimeDelta kMaxSignalingActiveInterval =
      base::Minutes(1);

  explicit ZombieHostDetector(base::OnceClosure on_zombie_state_detected);
  ~ZombieHostDetector() override;

  ZombieHostDetector(const ZombieHostDetector&) = delete;
  ZombieHostDetector& operator=(const ZombieHostDetector&) = delete;

  // Start monitoring zombie state.
  void Start();

  // HeartbeatSender::Observer implementations.
  void OnHeartbeatSent() override;

  // SignalingTracker implementations.
  void OnSignalingActive() override;

  base::TimeTicks GetNextDetectionTime() const;

 private:
  void CheckZombieState();

  base::OnceClosure on_zombie_state_detected_;

  base::RepeatingTimer check_zombie_state_timer_;
  base::TimeTicks last_heartbeat_time_;
  base::TimeTicks last_signaling_active_time_;
  bool previously_offline_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ZOMBIE_HOST_DETECTOR_H_