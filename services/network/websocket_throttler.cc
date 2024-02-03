// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_throttler.h"

#include <algorithm>

#include "base/rand_util.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {

constexpr int WebSocketPerProcessThrottler::kMaxPendingWebSocketConnections;

WebSocketPerProcessThrottler::PendingConnection::PendingConnection(
    base::WeakPtr<WebSocketPerProcessThrottler> throttler)
    : throttler_(std::move(throttler)) {
  DCHECK(throttler_);
  ++throttler_->num_pending_connections_;
}
WebSocketPerProcessThrottler::PendingConnection::PendingConnection(
    PendingConnection&& other)
    : throttler_(std::move(other.throttler_)) {
  other.throttler_ = nullptr;
}
WebSocketPerProcessThrottler::PendingConnection::~PendingConnection() {
  if (!throttler_)
    return;

  --throttler_->num_pending_connections_;
  ++throttler_->num_current_failed_connections_;
}

void WebSocketPerProcessThrottler::PendingConnection::OnCompleteHandshake() {
  DCHECK(throttler_);

  --throttler_->num_pending_connections_;
  ++throttler_->num_current_succeeded_connections_;
  throttler_ = nullptr;
}

WebSocketPerProcessThrottler::WebSocketPerProcessThrottler() {}
WebSocketPerProcessThrottler::~WebSocketPerProcessThrottler() {}

base::TimeDelta WebSocketPerProcessThrottler::CalculateDelay() const {
  int64_t f =
      num_previous_failed_connections_ + num_current_failed_connections_;
  int64_t s =
      num_previous_succeeded_connections_ + num_current_succeeded_connections_;
  int p = num_pending_connections_;
  return base::Milliseconds(base::RandInt(1000, 5000) *
                            (1 << std::min(p + f / (s + 1), INT64_C(16))) /
                            65536);
}

WebSocketPerProcessThrottler::PendingConnection
WebSocketPerProcessThrottler::IssuePendingConnectionTracker() {
  return PendingConnection(weak_factory_.GetWeakPtr());
}

bool WebSocketPerProcessThrottler::IsClean() const {
  return num_pending_connections_ == 0 &&
         num_current_succeeded_connections_ == 0 &&
         num_previous_succeeded_connections_ == 0 &&
         num_current_failed_connections_ == 0 &&
         num_previous_succeeded_connections_ == 0;
}

void WebSocketPerProcessThrottler::Roll() {
  num_previous_succeeded_connections_ = num_current_succeeded_connections_;
  num_previous_failed_connections_ = num_current_failed_connections_;

  num_current_succeeded_connections_ = 0;
  num_current_failed_connections_ = 0;
}

WebSocketThrottler::WebSocketThrottler() {}
WebSocketThrottler::~WebSocketThrottler() {}

bool WebSocketThrottler::HasTooManyPendingConnections(int process_id) const {
  auto it = per_process_throttlers_.find(process_id);
  if (it == per_process_throttlers_.end())
    return false;

  return it->second->HasTooManyPendingConnections();
}

base::TimeDelta WebSocketThrottler::CalculateDelay(int process_id) const {
  auto it = per_process_throttlers_.find(process_id);
  if (it == per_process_throttlers_.end())
    return base::TimeDelta();

  return it->second->CalculateDelay();
}

std::optional<WebSocketThrottler::PendingConnection>
WebSocketThrottler::IssuePendingConnectionTracker(int process_id) {
  if (process_id == mojom::kBrowserProcessId) {
    // The browser process is not throttled.
    return std::nullopt;
  }

  auto it = per_process_throttlers_.find(process_id);
  if (it == per_process_throttlers_.end()) {
    it = per_process_throttlers_
             .insert(std::make_pair(
                 process_id, std::make_unique<WebSocketPerProcessThrottler>()))
             .first;
  }

  if (!throttling_period_timer_.IsRunning()) {
    throttling_period_timer_.Start(FROM_HERE, base::Minutes(2), this,
                                   &WebSocketThrottler::OnTimer);
  }
  return it->second->IssuePendingConnectionTracker();
}

void WebSocketThrottler::OnTimer() {
  auto it = per_process_throttlers_.begin();
  while (it != per_process_throttlers_.end()) {
    it->second->Roll();
    if (it->second->IsClean()) {
      // We don't need the entry. Erase it.
      it = per_process_throttlers_.erase(it);
    } else {
      ++it;
    }
  }
  if (per_process_throttlers_.empty())
    throttling_period_timer_.Stop();
}

}  // namespace network
