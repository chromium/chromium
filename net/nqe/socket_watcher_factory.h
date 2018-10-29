// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_SOCKET_WATCHER_FACTORY_H_
#define NET_NQE_SOCKET_WATCHER_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"

namespace base {
class TickClock;
class TimeDelta;
}  // namespace base

namespace net {

namespace {

typedef base::Callback<void(SocketPerformanceWatcherFactory::Protocol protocol,
                            const base::TimeDelta& rtt,
                            const base::Optional<nqe::internal::IPHash>& host)>
    OnUpdatedRTTAvailableCallback;

typedef base::Callback<bool(base::TimeTicks)> ShouldNotifyRTTCallback;

}  // namespace

namespace nqe {

namespace internal {

// SocketWatcherFactory implements SocketPerformanceWatcherFactory.
// SocketWatcherFactory is thread safe.
class SocketWatcherFactory : public SocketPerformanceWatcherFactory {
 public:
  // Creates a SocketWatcherFactory.  All socket watchers created by
  // SocketWatcherFactory call |updated_rtt_observation_callback| on
  // |task_runner| every time a new RTT observation is available.
  // |min_notification_interval| is the minimum interval betweeen consecutive
  // notifications to the socket watchers created by this factory. |tick_clock|
  // is guaranteed to be non-null. |should_notify_rtt_callback| is the callback
  // that should be called back on |task_runner| to check if RTT observation
  // should be taken and notified.
  SocketWatcherFactory(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::TimeDelta min_notification_interval,
      OnUpdatedRTTAvailableCallback updated_rtt_observation_callback,
      ShouldNotifyRTTCallback should_notify_rtt_callback,
      const base::TickClock* tick_clock);

  ~SocketWatcherFactory() override;

  // SocketPerformanceWatcherFactory implementation:
  std::unique_ptr<SocketPerformanceWatcher> CreateSocketPerformanceWatcher(
      const Protocol protocol,
      const AddressList& address_list) override;

  void SetUseLocalHostRequestsForTesting(bool use_localhost_requests) {
    allow_rtt_private_address_ = use_localhost_requests;
  }

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Minimum interval betweeen consecutive notifications to the socket watchers
  // created by this factory.
  const base::TimeDelta min_notification_interval_;

  // True if socket watchers constructed by this factory can use the RTT from
  // the sockets that are connected to the private addresses.
  bool allow_rtt_private_address_;

  // Called every time a new RTT observation is available.
  OnUpdatedRTTAvailableCallback updated_rtt_observation_callback_;

  // Callback that should be called by socket watchers to determine if the RTT
  // notification should be notified using |updated_rtt_observation_callback_|.
  ShouldNotifyRTTCallback should_notify_rtt_callback_;

  const base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(SocketWatcherFactory);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_SOCKET_WATCHER_FACTORY_H_
