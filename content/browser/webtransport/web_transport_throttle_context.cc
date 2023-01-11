// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/web_transport_throttle_context.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"

namespace content {

WebTransportThrottleContext::Tracker::Tracker(
    base::WeakPtr<WebTransportThrottleContext> throttle_context)
    : throttle_context_(throttle_context) {
  DVLOG(1) << "WebTransportThrottleContext::Tracker()"
           << " this=" << this << " pending_handshakes_= "
           << throttle_context_->pending_handshakes_;
  DCHECK(throttle_context_);
  DCHECK_LT(throttle_context_->pending_handshakes_, kMaxPendingSessions);
  ++throttle_context_->pending_handshakes_;
}

WebTransportThrottleContext::Tracker::~Tracker() {
  if (throttle_context_) {
    throttle_context_->QueuePending(base::Minutes(5));
  }
}

void WebTransportThrottleContext::Tracker::OnHandshakeEstablished() {
  DVLOG(1) << "WebTransportThrottleContext::Tracker::OnHandshakeEstablished()"
           << " this=" << this;

  if (!throttle_context_)
    return;

  DVLOG(1) << "    pending_handshakes_= "
           << throttle_context_->pending_handshakes_;
  DCHECK_GT(throttle_context_->pending_handshakes_, 0);
  throttle_context_->QueuePending(base::Milliseconds(10));
  throttle_context_ = nullptr;
}

void WebTransportThrottleContext::Tracker::OnHandshakeFailed() {
  DVLOG(1) << "WebTransportThrottleContext::Tracker::OnHandshakeFailed()"
           << " this=" << this;

  if (!throttle_context_)
    return;

  DVLOG(1) << "    pending_handshakes_= "
           << throttle_context_->pending_handshakes_;
  DCHECK_GT(throttle_context_->pending_handshakes_, 0);
  throttle_context_->QueuePending(base::Minutes(5));
  throttle_context_ = nullptr;
}

WebTransportThrottleContext::WebTransportThrottleContext() = default;
WebTransportThrottleContext::~WebTransportThrottleContext() = default;

WebTransportThrottleContext::ThrottleResult
WebTransportThrottleContext::PerformThrottle(
    ThrottleDoneCallback on_throttle_done) {
  DVLOG(1) << "WebTransportThrottleContext::PerformThrottle() this=" << this
           << " pending_handshakes_=" << pending_handshakes_;

  if (!pending_queue_timer_.IsRunning()) {
    // If the timer was not running there may be some pending connections that
    // were not cleaned up yet. May cause other handshakes to be started as a
    // side-effect, but since they are unrelated this is harmless.
    MaybeDecrementPending();
  }

  if (pending_handshakes_ + static_cast<int>(throttled_connections_.size()) >=
      kMaxPendingSessions) {
    return ThrottleResult::kTooManyPendingSessions;
  }

  throttled_connections_.push(std::move(on_throttle_done));
  if (!throttled_connections_timer_.IsRunning()) {
    queue_head_time_ = base::TimeTicks::Now();
    ScheduleThrottledConnection();
  }

  if (!pending_queue_timer_.IsRunning() && !pending_queue_.empty() &&
      !throttled_connections_.empty()) {
    StartPendingQueueTimer(pending_queue_.top() - base::TimeTicks::Now());
  }

  return ThrottleResult::kOk;
}

base::WeakPtr<WebTransportThrottleContext>
WebTransportThrottleContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebTransportThrottleContext::ScheduleThrottledConnection() {
  DVLOG(1) << "WebTransportThrottleContext::ScheduleThrottledConnection() this="
           << this << " pending_handshakes_= " << pending_handshakes_;

  DCHECK(!throttled_connections_.empty());

  if (pending_handshakes_ == 0) {
    DoOnThrottleDone();
    return;
  }

  DCHECK_GT(pending_handshakes_, 0);

  // Don't do the calculation for large values of `pending_handshakes_` to avoid
  // integer overflow. If `pending_handshakes_` is 14, the result of the
  // calculation is 81920, so it will always get truncated to 60000.
  const int milliseconds_delay =
      pending_handshakes_ > 13
          ? 60000
          : std::min(10 * (1 << (pending_handshakes_ - 1)), 60000);

  // We multiply the timeout by a random factor so that when a server falls over
  // and the client code starts to accidentally DoS it, all the clients don't
  // arrive at the same time when it recovers.
  const double random_multiplier = base::RandDouble() + 0.5;

  const base::TimeDelta delay =
      base::Milliseconds(milliseconds_delay * random_multiplier);
  DCHECK_GT(delay, base::Seconds(0));
  const base::TimeTicks when = queue_head_time_ + delay;
  const base::TimeDelta relative_delay = when - base::TimeTicks::Now();

  if (relative_delay <= base::Seconds(0)) {
    DVLOG(1) << "relative_delay=" << relative_delay << " so firing immediately";
    DoOnThrottleDone();
    return;
  }

  DVLOG(1) << "Starting throttled_connections_timer_ with delay="
           << relative_delay;
  // It is safe to use base::Unretained here because the timer is owned by this
  // object and will be stopped if this object is destroyed.
  throttled_connections_timer_.Start(
      FROM_HERE, relative_delay,
      base::BindOnce(&WebTransportThrottleContext::StartOneConnection,
                     base::Unretained(this)));
}

void WebTransportThrottleContext::DoOnThrottleDone() {
  DVLOG(1) << "WebTransportThrottleContext::DoOnThrottleDone() this=" << this
           << " pending_handshakes_= " << pending_handshakes_
           << " throttled_connections_.size()="
           << throttled_connections_.size();
  DCHECK(!throttled_connections_.empty());
  auto on_throttle_done = std::move(throttled_connections_.front());
  throttled_connections_.pop();
  queue_head_time_ = base::TimeTicks::Now();
  if (throttled_connections_.empty() && pending_queue_timer_.IsRunning()) {
    pending_queue_timer_.Stop();
  }
  auto tracker = std::make_unique<Tracker>(GetWeakPtr());
  std::move(on_throttle_done).Run(std::move(tracker));
}

void WebTransportThrottleContext::StartOneConnection() {
  DVLOG(1) << "WebTransportThrottleContext::StartOneConnection() this=" << this
           << " pending_handshakes_= " << pending_handshakes_;

  if (throttled_connections_.empty())
    return;
  DoOnThrottleDone();
  if (!throttled_connections_.empty()) {
    ScheduleThrottledConnection();
  }
}

void WebTransportThrottleContext::QueuePending(base::TimeDelta after) {
  DVLOG(1) << "WebTransportThrottleContext::QueuePending() this=" << this
           << " after=" << after
           << " pending_handshakes_= " << pending_handshakes_;

  const auto when = base::TimeTicks::Now() + after;
  if (!throttled_connections_.empty() &&
      (pending_queue_.empty() || when < pending_queue_.top())) {
    StartPendingQueueTimer(after);
  }
  pending_queue_.push(when);
}

void WebTransportThrottleContext::MaybeDecrementPending() {
  DVLOG(1) << "WebTransportThrottleContext::MaybeDecrementPending() this="
           << this << " pending_handshakes_= " << pending_handshakes_;

  const auto now = base::TimeTicks::Now();
  while (!pending_queue_.empty() && pending_queue_.top() <= now) {
    pending_queue_.pop();
    --pending_handshakes_;
  }
  if (!throttled_connections_.empty()) {
    ScheduleThrottledConnection();
  }

  if (!pending_queue_.empty() && !throttled_connections_.empty()) {
    StartPendingQueueTimer(pending_queue_.top() - base::TimeTicks::Now());
  }
}

void WebTransportThrottleContext::StartPendingQueueTimer(
    base::TimeDelta after) {
  DVLOG(1) << "WebTransportThrottleContext::StartPendingQueueTimer() this="
           << this << " after=" << after
           << " pending_handshakes_= " << pending_handshakes_;

  DCHECK(!throttled_connections_.empty());
  // This use of base::Unretained is safe because this timer is owned by this
  // object and will be stopped on destruction.
  pending_queue_timer_.Start(
      FROM_HERE, after,
      base::BindOnce(&WebTransportThrottleContext::MaybeDecrementPending,
                     base::Unretained(this)));
}

}  // namespace content
