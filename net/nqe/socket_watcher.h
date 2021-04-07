// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_SOCKET_WATCHER_H_
#define NET_NQE_SOCKET_WATCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
class TimeDelta;
}  // namespace base

namespace net {

class AddressList;

namespace {

typedef base::RepeatingCallback<void(
    SocketPerformanceWatcherFactory::Protocol protocol,
    const base::TimeDelta& rtt,
    const base::Optional<nqe::internal::IPHash>& host)>
    OnUpdatedRTTAvailableCallback;

typedef base::RepeatingCallback<bool(base::TimeTicks)> ShouldNotifyRTTCallback;

}  // namespace

namespace nqe {

namespace internal {

// SocketWatcher implements SocketPerformanceWatcher, and is not thread-safe.
class NET_EXPORT_PRIVATE SocketWatcher : public SocketPerformanceWatcher {
 public:
  // Creates a SocketWatcher which can be used to watch a socket that uses
  // |protocol| as the transport layer protocol. The socket watcher will call
  // |updated_rtt_observation_callback| on |task_runner| every time a new RTT
  // observation is available. |address_list| is the list of addresses that
  // the socket may connect to. |min_notification_interval| is the minimum
  // interval betweeen consecutive notifications to this socket watcher.
  // |allow_rtt_private_address| is true if |updated_rtt_observation_callback|
  // should be called when RTT observation from a socket connected to private
  // address is received. |tick_clock| is guaranteed to be non-null.
  // |should_notify_rtt_callback| callback should be called back on
  // |task_runner| by the created socket watchers to check if RTT observation
  // should be taken and notified.
  SocketWatcher(SocketPerformanceWatcherFactory::Protocol protocol,
                const AddressList& address_list,
                base::TimeDelta min_notification_interval,
                bool allow_rtt_private_address,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                OnUpdatedRTTAvailableCallback updated_rtt_observation_callback,
                ShouldNotifyRTTCallback should_notify_rtt_callback,
                const base::TickClock* tick_clock);

  ~SocketWatcher() override;

  // SocketPerformanceWatcher implementation:
  bool ShouldNotifyUpdatedRTT() const override;
  void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) override;
  void OnConnectionChanged() override;

 private:
  // Transport layer protocol used by the socket that |this| is watching.
  const SocketPerformanceWatcherFactory::Protocol protocol_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Called every time a new RTT observation is available.
  OnUpdatedRTTAvailableCallback updated_rtt_observation_callback_;

  // Called to determine if the RTT notification should be notified using
  // |updated_rtt_observation_callback_|.
  ShouldNotifyRTTCallback should_notify_rtt_callback_;

  // Minimum interval betweeen consecutive incoming notifications.
  const base::TimeDelta rtt_notifications_minimum_interval_;

  // True if the RTT observations from this socket can be notified using
  // |updated_rtt_observation_callback_|.
  const bool run_rtt_callback_;

  // Time when this was last notified of updated RTT.
  base::TimeTicks last_rtt_notification_;

  const base::TickClock* tick_clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  // True if the first RTT notification from the QUIC connection has been
  // received.
  bool first_quic_rtt_notification_received_;

  // A unique identifier for the remote host that this socket connects to.
  const base::Optional<IPHash> host_;

  DISALLOW_COPY_AND_ASSIGN(SocketWatcher);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_SOCKET_WATCHER_H_
