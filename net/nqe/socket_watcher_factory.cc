// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/socket_watcher_factory.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/nqe/socket_watcher.h"

namespace net::nqe::internal {

SocketWatcherFactory::SocketWatcherFactory(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TimeDelta min_notification_interval,
    OnUpdatedRTTAvailableCallback updated_rtt_observation_callback,
    ShouldNotifyRTTCallback should_notify_rtt_callback,
    const base::TickClock* tick_clock)
    : task_runner_(std::move(task_runner)),
      min_notification_interval_(min_notification_interval),
      updated_rtt_observation_callback_(updated_rtt_observation_callback),
      should_notify_rtt_callback_(should_notify_rtt_callback),
      tick_clock_(tick_clock) {
  DCHECK(tick_clock_);
}

SocketWatcherFactory::~SocketWatcherFactory() = default;

std::unique_ptr<SocketPerformanceWatcher>
SocketWatcherFactory::CreateSocketPerformanceWatcher(const Protocol protocol,
                                                     const IPAddress& address) {
  return std::make_unique<SocketWatcher>(
      protocol, address, min_notification_interval_, allow_rtt_private_address_,
      task_runner_, updated_rtt_observation_callback_,
      should_notify_rtt_callback_, tick_clock_);
}

void SocketWatcherFactory::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

}  // namespace net::nqe::internal
