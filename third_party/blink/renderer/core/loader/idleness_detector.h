// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IDLENESS_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IDLENESS_DETECTOR_H_

#include "base/macros.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class LocalFrame;
class ResourceFetcher;

// IdlenessDetector observes the resource request count every time a load is
// finshed after DOMContentLoadedEventEnd is fired. It emits a network almost
// idle signal when there are no more than 2 network connections active in 0.5
// seconds, and a network idle signal when there are 0 network connections
// active in 0.5 seconds.
class CORE_EXPORT IdlenessDetector
    : public GarbageCollected<IdlenessDetector>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  IdlenessDetector(
      LocalFrame*,
      const base::TickClock* = base::DefaultTickClock::GetInstance());

  void Shutdown();
  void WillCommitLoad();
  void DomContentLoadedEventFired();
  // TODO(lpy) Don't need to pass in fetcher once the command line of disabling
  // PlzNavigate is removed.
  void OnWillSendRequest(ResourceFetcher*);
  void OnDidLoadResource();

  base::TimeTicks GetNetworkAlmostIdleTime();
  base::TimeTicks GetNetworkIdleTime();
  bool NetworkIsAlmostIdle();

  void Trace(blink::Visitor*);

 private:
  friend class IdlenessDetectorTest;

  // The page is quiet if there are no more than 2 active network requests for
  // this duration of time.
  static constexpr base::TimeDelta kNetworkQuietWindow =
      base::TimeDelta::FromMilliseconds(500);
  static constexpr base::TimeDelta kNetworkQuietWatchdog =
      base::TimeDelta::FromSeconds(2);
  static constexpr int kNetworkQuietMaximumConnections = 2;

  // TaskTimeObserver implementation.
  void WillProcessTask(base::TimeTicks start_time) override;
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override;

  void Stop();

  // This method and the associated timer appear to have no effect, but they
  // have the side effect of triggering a task, which will send WillProcessTask
  // and DidProcessTask observer notifications.
  void NetworkQuietTimerFired(TimerBase*);

  Member<LocalFrame> local_frame_;
  bool task_observer_added_;

  bool in_network_0_quiet_period_ = true;
  bool in_network_2_quiet_period_ = true;

  const base::TickClock* clock_;

  base::TimeDelta network_quiet_window_ = kNetworkQuietWindow;
  // Store the accumulated time of network quiet.
  base::TimeTicks network_0_quiet_;
  base::TimeTicks network_2_quiet_;
  // Record the actual start time of network quiet.
  base::TimeTicks network_0_quiet_start_time_;
  base::TimeTicks network_2_quiet_start_time_;
  TaskRunnerTimer<IdlenessDetector> network_quiet_timer_;

  DISALLOW_COPY_AND_ASSIGN(IdlenessDetector);
};

}  // namespace blink

#endif
