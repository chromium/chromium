// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/reconnect_notifier.h"

#include <cstdint>
#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_constants.h"

namespace net {

namespace {

bool IsValidNonNegativeFiniteTimeDelta(base::TimeDelta delta) {
  return !delta.is_negative() && !delta.is_inf();
}

}  // namespace

ConnectionChangeNotifier::Observer::Observer() = default;
ConnectionChangeNotifier::Observer::~Observer() {
  if (notifier_) {
    notifier_->RemoveObserver(this);
  }
}

void ConnectionChangeNotifier::Observer::OnAttach(
    base::WeakPtr<ConnectionChangeNotifier> notifier) {
  CHECK(!notifier_);
  notifier_ = notifier;
}

base::WeakPtr<ConnectionChangeNotifier::Observer>
ConnectionChangeNotifier::Observer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

ConnectionChangeNotifier::EstablishedConnectionInfo::
    EstablishedConnectionInfo() = default;
ConnectionChangeNotifier::EstablishedConnectionInfo::
    ~EstablishedConnectionInfo() = default;
ConnectionChangeNotifier::EstablishedConnectionInfo::EstablishedConnectionInfo(
    const EstablishedConnectionInfo& other) = default;
ConnectionChangeNotifier::EstablishedConnectionInfo&
ConnectionChangeNotifier::EstablishedConnectionInfo::operator=(
    const EstablishedConnectionInfo& other) = default;

void ConnectionChangeNotifier::OnConnectionEstablished(
    const EstablishedConnectionInfo& info) {
  observer_list_.Notify(
      &ConnectionChangeNotifier::Observer::OnConnectionEstablished, info);
}

void ConnectionChangeNotifier::OnSessionClosed(
    bool was_ever_used_to_create_streams) {
  observer_list_.Notify(&ConnectionChangeNotifier::Observer::OnSessionClosed,
                        was_ever_used_to_create_streams);
}

void ConnectionChangeNotifier::OnConnectionFailed() {
  observer_list_.Notify(
      &ConnectionChangeNotifier::Observer::OnConnectionFailed);
}

void ConnectionChangeNotifier::OnNetworkEvent(NetworkChangeEvent event) {
  observer_list_.Notify(&ConnectionChangeNotifier::Observer::OnNetworkEvent,
                        event);
}

void ConnectionChangeNotifier::AddObserver(
    ConnectionChangeNotifier::Observer* observer) {
  CHECK(observer);
  // If the observer is already in the observer list, then we should not add it
  // again. This might happen when racing the Http2 and Http3 creation (in
  // HttpStreamFactoryJob), and both connections were successfully established.
  // TODO(crbug.com/453308537): Update to prioritize the Http3 connection's
  // observer if it is already in the observer list.
  if (observer->IsInObserverList()) {
    return;
  }
  observer_list_.AddObserver(observer);

  // Set the WeakPtr of `this` so that the observer can remove itself when
  // it is being destructed.
  observer->OnAttach(weak_factory_.GetWeakPtr());
}

void ConnectionChangeNotifier::RemoveObserver(const Observer* observer) {
  CHECK(observer);
  observer_list_.RemoveObserver(observer);
}

ConnectionChangeNotifier::ConnectionChangeNotifier() = default;
ConnectionChangeNotifier::~ConnectionChangeNotifier() = default;

bool ConnectionKeepAliveConfig::IsValid() const {
  if (!IsValidNonNegativeFiniteTimeDelta(idle_timeout) ||
      !IsValidNonNegativeFiniteTimeDelta(ping_interval) ||
      !IsValidNonNegativeFiniteTimeDelta(initial_rtt)) {
    return false;
  }

  if (preconnect_priority < RequestPriority::IDLE ||
      preconnect_priority > RequestPriority::MAXIMUM_PRIORITY) {
    return false;
  }

  // `QuicPingManager` schedules keep-alive PINGs at >= 1s granularity.
  if (!ping_interval.is_zero() && ping_interval < base::Seconds(1)) {
    return false;
  }

  // `QuicConnection::SetNetworkTimeouts` subtracts 1s from the client's local
  // idle timeout and `QuicAlarmMultiplexer::FireAlarms` runs the idle alarm
  // before the ping alarm on equal deadlines; capping at `UINT32_MAX` seconds
  // keeps `QuicConfig` storage and `QuicTime` deadline arithmetic
  // overflow-safe.
  if (idle_timeout.is_zero()) {
    return ping_interval.is_zero();
  }
  if (idle_timeout < base::Seconds(2) ||
      idle_timeout > base::Seconds(std::numeric_limits<uint32_t>::max())) {
    return false;
  }
  const base::TimeDelta effective_ping_interval =
      ping_interval.is_zero() ? base::Seconds(quic::kPingTimeoutSecs)
                              : ping_interval;
  return effective_ping_interval < idle_timeout - base::Seconds(1);
}

ConnectionManagementConfig::ConnectionManagementConfig() = default;
ConnectionManagementConfig::~ConnectionManagementConfig() = default;

ConnectionManagementConfig::ConnectionManagementConfig(
    const ConnectionManagementConfig& other) = default;

ConnectionManagementConfig::ConnectionManagementConfig(
    ConnectionManagementConfig&& other) = default;
}  // namespace net
