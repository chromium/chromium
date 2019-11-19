// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/socket_watcher.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

// Generate a compact representation for the first IP in |address_list|. For
// IPv4, all 32 bits are used and for IPv6, the first 64 bits are used as the
// remote host identifier.
base::Optional<IPHash> CalculateIPHash(const AddressList& address_list) {
  if (address_list.empty())
    return base::nullopt;

  const IPAddress& ip_addr = address_list.front().address();

  IPAddressBytes bytes = ip_addr.bytes();

  // For IPv4, the first four bytes are taken. For IPv6, the first 8 bytes are
  // taken. For IPv4MappedIPv6, the last 4 bytes are taken.
  int index_min = ip_addr.IsIPv4MappedIPv6() ? 12 : 0;
  int index_max;
  if (ip_addr.IsIPv4MappedIPv6())
    index_max = 16;
  else
    index_max = ip_addr.IsIPv4() ? 4 : 8;

  DCHECK_LE(index_min, index_max);
  DCHECK_GE(8, index_max - index_min);

  uint64_t result = 0ULL;
  for (int i = index_min; i < index_max; ++i) {
    result = result << 8;
    result |= bytes[i];
  }
  return result;
}

}  // namespace

SocketWatcher::SocketWatcher(
    SocketPerformanceWatcherFactory::Protocol protocol,
    const AddressList& address_list,
    base::TimeDelta min_notification_interval,
    bool allow_rtt_private_address,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    OnUpdatedRTTAvailableCallback updated_rtt_observation_callback,
    ShouldNotifyRTTCallback should_notify_rtt_callback,
    const base::TickClock* tick_clock)
    : protocol_(protocol),
      task_runner_(std::move(task_runner)),
      updated_rtt_observation_callback_(updated_rtt_observation_callback),
      should_notify_rtt_callback_(should_notify_rtt_callback),
      rtt_notifications_minimum_interval_(min_notification_interval),
      run_rtt_callback_(allow_rtt_private_address ||
                        (!address_list.empty() &&
                         address_list.front().address().IsPubliclyRoutable())),
      tick_clock_(tick_clock),
      first_quic_rtt_notification_received_(false),
      host_(CalculateIPHash(address_list)) {
  DCHECK(tick_clock_);
  DCHECK(last_rtt_notification_.is_null());
}

SocketWatcher::~SocketWatcher() = default;

bool SocketWatcher::ShouldNotifyUpdatedRTT() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!run_rtt_callback_)
    return false;

  const base::TimeTicks now = tick_clock_->NowTicks();

  if (task_runner_->RunsTasksInCurrentSequence()) {
    // Enables socket watcher to send more frequent RTT observations when very
    // few sockets are receiving data.
    if (should_notify_rtt_callback_.Run(now))
      return true;
  }

  // Do not allow incoming notifications if the last notification was more
  // recent than |rtt_notifications_minimum_interval_| ago. This helps in
  // reducing the overhead of obtaining the RTT values.
  // Enables a socket watcher to send RTT observation, helps in reducing
  // starvation by allowing every socket watcher to notify at least one RTT
  // notification every |rtt_notifications_minimum_interval_| duration.
  return now - last_rtt_notification_ >= rtt_notifications_minimum_interval_;
}

void SocketWatcher::OnUpdatedRTTAvailable(const base::TimeDelta& rtt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // tcp_socket_posix may sometimes report RTT as 1 microsecond when the RTT was
  // actually invalid. See:
  // https://cs.chromium.org/chromium/src/net/socket/tcp_socket_posix.cc?rcl=7ad660e34f2a996e381a85b2a515263003b0c171&l=106.
  if (rtt <= base::TimeDelta::FromMicroseconds(1))
    return;

  if (!first_quic_rtt_notification_received_ &&
      protocol_ == SocketPerformanceWatcherFactory::PROTOCOL_QUIC) {
    // First RTT sample from QUIC connections may be synthetically generated,
    // and may not reflect the actual network quality.
    first_quic_rtt_notification_received_ = true;
    return;
  }

  last_rtt_notification_ = tick_clock_->NowTicks();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(updated_rtt_observation_callback_, protocol_, rtt, host_));
}

void SocketWatcher::OnConnectionChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
