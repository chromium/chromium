// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/web_transport_throttle_context.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "net/base/features.h"

namespace content {

namespace {

bool IsFineGrainedThrottlingEnabled() {
  return base::FeatureList::IsEnabled(
      net::features::kWebTransportFineGrainedThrottling);
}

bool ShouldQueueHandshakeFailurePenalty() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line ||
         !command_line->HasSwitch(switches::kWebTransportDeveloperMode);
}

std::optional<net::IPAddress> GetSubnetAddress(const net::IPAddress& address) {
  // We don't have a way to get the actual subnet mask, so assuming /24 and /64
  // for IPv4 and IPv6 respectively.
  if (!address.IsValid()) {
    return std::nullopt;
  }
  size_t size = address.IsIPv4() ? 4 : 16;
  size_t prefix_bytes = address.IsIPv4() ? 3 : 8;
  base::span<const uint8_t> raw_bytes = address.bytes();
  std::array<uint8_t, 16> subnet_bytes = {};
  DCHECK_GE(raw_bytes.size(), prefix_bytes);
  base::span(subnet_bytes).copy_prefix_from(raw_bytes.first(prefix_bytes));

  return net::IPAddress(base::span<uint8_t>(subnet_bytes).first(size));
}

}  // namespace

static constexpr base::TimeDelta kFailureForgivenessDuration =
    base::Minutes(15);

WebTransportThrottleContext::PenaltyManager::PenaltyManager(
    WebTransportThrottleContext* throttle_context)
    : throttle_context_(throttle_context) {}

WebTransportThrottleContext::PenaltyManager::~PenaltyManager() = default;

void WebTransportThrottleContext::PenaltyManager::CleanupFailedHandshakes() {
  auto threshold = base::TimeTicks::Now() - kFailureForgivenessDuration;
  std::erase_if(failed_handshakes_, [threshold](const auto& item) {
    const auto& [_, last_failure_time] = item;
    return last_failure_time <= threshold;
  });

  if (failed_handshakes_.empty()) {
    failed_handshakes_timer_.Stop();
  }
}

bool WebTransportThrottleContext::PenaltyManager::FailedHandshakeNeedsPenalty(
    const net::IPAddress ip_address) {
  auto now = base::TimeTicks::Now();
  auto insert_result = failed_handshakes_.try_emplace(ip_address, now);
  // The first failure doesn't cause penalty.
  if (insert_result.second) {
    return false;
  }
  auto it = insert_result.first;
  auto threshold = now - kFailureForgivenessDuration;
  // An obsolete failure doesn't cause penalty
  bool needs_penalty = it->second > threshold;
  failed_handshakes_[ip_address] = now;
  return needs_penalty;
}

base::TimeDelta
WebTransportThrottleContext::PenaltyManager::ComputeHandshakePenalty(
    const std::optional<net::IPAddress>& server_address) {
  DVLOG(1) << "WebTransportThrottleContext::ComputeHandshakePenalty() this="
           << this;

  if (!failed_handshakes_timer_.IsRunning()) {
    failed_handshakes_timer_.Start(
        FROM_HERE, base::Minutes(5),
        base::BindRepeating(&PenaltyManager::CleanupFailedHandshakes,
                            base::Unretained(this)));
  }

  if (!server_address) {
    // This only happens if the Web Transport Server use a domain name and the
    // connection is cancelled before the DNS request is completed.
    if (FailedHandshakeNeedsPenalty(net::IPAddress())) {
      DVLOG(1) << "Return max penalty when several requests to unknown server "
                  "address are cancelled abruptly.";
      return base::Minutes(5);
    }
    DVLOG(1) << "Return min penalty for a requested cancelled before the "
                "handshake was completed.";
    return base::Milliseconds(50);
  }

  if (FailedHandshakeNeedsPenalty(*server_address)) {
    DVLOG(1) << "Return max penalty for a request targeting the "
             << server_address->ToString() << " host and failed several times.";
    return base::Minutes(5);
  }

  auto net_address = GetSubnetAddress(*server_address);
  if (net_address) {
    if (FailedHandshakeNeedsPenalty(*net_address)) {
      DVLOG(1) << "Return mid penalty for a request targeting the "
               << net_address->ToString()
               << " subnet and failed several times.";
      return base::Minutes(2);
    }
  }

  DVLOG(1) << "Return default penalty for a request that target "
           << server_address->ToString() << " and failed for the first time.";
  return base::Milliseconds(100);
}

void WebTransportThrottleContext::PenaltyManager::QueuePending(
    base::TimeDelta after) {
  DVLOG(1) << "WebTransportThrottleContext::QueuePending() this=" << this
           << " after=" << after
           << " pending_handshakes_= " << pending_handshakes_;

  const auto when = base::TimeTicks::Now() + after;
  if (pending_queue_.empty() || when < pending_queue_.top()) {
    StartPendingQueueTimer(after);
  }
  pending_queue_.push(when);
}

void WebTransportThrottleContext::PenaltyManager::MaybeDecrementPending() {
  DVLOG(1) << "WebTransportThrottleContext::MaybeDecrementPending() this="
           << this << " pending_handshakes_= " << pending_handshakes_;

  const auto now = base::TimeTicks::Now();
  while (!pending_queue_.empty() && pending_queue_.top() <= now) {
    pending_queue_.pop();
    --pending_handshakes_;
  }
  throttle_context_->OnPendingQueueReady();

  ProcessPendingQueue();
}

void WebTransportThrottleContext::PenaltyManager::ProcessPendingQueue() {
  if (pending_queue_.empty()) {
    return;
  }

  StartPendingQueueTimer(pending_queue_.top() - base::TimeTicks::Now());
}

void WebTransportThrottleContext::PenaltyManager::StopPendingQueueTimer() {
  if (pending_queue_timer_.IsRunning()) {
    pending_queue_timer_.Stop();
  }
}

void WebTransportThrottleContext::PenaltyManager::StartPendingQueueTimer(
    base::TimeDelta after) {
  DVLOG(1) << "WebTransportThrottleContext::StartPendingQueueTimer() this="
           << this << " after=" << after
           << " pending_handshakes_= " << pending_handshakes_;

  // This use of base::Unretained is safe because this timer is owned by this
  // object and will be stopped on destruction.
  pending_queue_timer_.Start(
      FROM_HERE, after,
      base::BindOnce(&PenaltyManager::MaybeDecrementPending,
                     base::Unretained(this)));
}

WebTransportThrottleContext::Tracker::Tracker(
    base::WeakPtr<WebTransportThrottleContext> throttle_context)
    : throttle_context_(throttle_context) {
  DVLOG(1) << "WebTransportThrottleContext::Tracker()" << " this=" << this
           << " pending_handshakes_= "
           << throttle_context_->penalty_mgr_.PendingHandshakes();
  DCHECK(throttle_context_);
  DCHECK_LT(throttle_context_->penalty_mgr_.PendingHandshakes(),
            kMaxPendingSessions);
  throttle_context_->penalty_mgr_.AddPendingHandshakes();
}

WebTransportThrottleContext::Tracker::~Tracker() {
  if (!throttle_context_) {
    // The penalty has been already computed based on the handshake result.
    return;
  }

  if (!throttle_done_) {
    // The request was cancelled during the throttling stage, before any network
    // activity was performed. This can provide no benefit to an attacker, so
    // there is no need to penalize it.
    throttle_context_->RemovePendingHandshakes();
    return;
  }

  // Handle early-cancellation scenarios, where the connection is cancelled
  // before getting the handshake result.

  if (server_address_.IsValid()) {
    // Connection cancelled by the network process after throttling targeting a
    // valid ip address, either because the DNS request has been resolved or
    // because the URL's host was alreadu a valid IP address.
    throttle_context_->MaybeQueueHandshakeFailurePenalty(server_address_);
    return;
  }

  // Connection cancelled after throttling but before the DNS request has been
  // resolved.
  throttle_context_->MaybeQueueHandshakeFailurePenalty(std::nullopt);
}

void WebTransportThrottleContext::Tracker::SetServerAddress(
    const net::IPAddress& server_address) {
  DVLOG(1) << "WebTransportThrottleContext::Tracker::OnBeforeConnect()"
           << " server_address= " << server_address.ToString();

  if (server_address.IsValid()) {
    server_address_ = server_address;
  }
}

void WebTransportThrottleContext::Tracker::OnHandshakeEstablished() {
  DVLOG(1) << "WebTransportThrottleContext::Tracker::OnHandshakeEstablished()"
           << " this=" << this;

  if (!throttle_context_)
    return;

  DVLOG(1) << "    pending_handshakes_= "
           << throttle_context_->penalty_mgr_.PendingHandshakes();
  DCHECK_GT(throttle_context_->penalty_mgr_.PendingHandshakes(), 0);
  throttle_context_->penalty_mgr_.QueuePending(base::Milliseconds(10));
  throttle_context_ = nullptr;
}

void WebTransportThrottleContext::Tracker::OnHandshakeFailed() {
  DVLOG(1) << "WebTransportThrottleContext::Tracker::OnHandshakeFailed()"
           << " this=" << this;

  if (!throttle_context_)
    return;

  DVLOG(1) << "    pending_handshakes_= "
           << throttle_context_->penalty_mgr_.PendingHandshakes();
  throttle_context_->MaybeQueueHandshakeFailurePenalty(server_address_);
  throttle_context_ = nullptr;
}

WebTransportThrottleContext::WebTransportThrottleContext()
    : should_queue_handshake_failure_penalty_(
          ShouldQueueHandshakeFailurePenalty()) {}

WebTransportThrottleContext::~WebTransportThrottleContext() = default;

WebTransportThrottleContext::ThrottleResult
WebTransportThrottleContext::PerformThrottle(
    ThrottleDoneCallback on_throttle_done) {
  DVLOG(1) << "WebTransportThrottleContext::PerformThrottle() this=" << this
           << " pending_handshakes_=" << penalty_mgr_.PendingHandshakes()
           << " throttled_connections_=" << throttled_connections_.size();

  if (!penalty_mgr_.PendingQueueTimerIsRunning()) {
    // If the timer was not running there may be some pending connections that
    // were not cleaned up yet. May cause other handshakes to be started as a
    // side-effect, but since they are unrelated this is harmless.
    penalty_mgr_.MaybeDecrementPending();
  }

  if (penalty_mgr_.PendingHandshakes() +
          static_cast<int>(throttled_connections_.size()) >=
      kMaxPendingSessions) {
    DVLOG(1) << "WebTransportThrottleContext::PerformThrottle() -- Too many "
                "connections !!!";
    return ThrottleResult::kTooManyPendingSessions;
  }

  throttled_connections_.push(std::move(on_throttle_done));
  if (!throttled_connections_timer_.IsRunning()) {
    queue_head_time_ = base::TimeTicks::Now();
    ScheduleThrottledConnection();
  }

  if (!penalty_mgr_.PendingQueueTimerIsRunning() &&
      !throttled_connections_.empty()) {
    penalty_mgr_.ProcessPendingQueue();
  }

  return ThrottleResult::kOk;
}

void WebTransportThrottleContext::MaybeQueueHandshakeFailurePenalty(
    const std::optional<net::IPAddress>& server_address) {
  if (should_queue_handshake_failure_penalty_) {
    auto penalty = base::Minutes(5);
    if (IsFineGrainedThrottlingEnabled()) {
      penalty = penalty_mgr_.ComputeHandshakePenalty(server_address);
    }
    penalty_mgr_.QueuePending(penalty);
    return;
  }
  CHECK_GE(penalty_mgr_.PendingHandshakes(), 0);
  penalty_mgr_.RemovePendingHandshakes();
}

base::WeakPtr<WebTransportThrottleContext>
WebTransportThrottleContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebTransportThrottleContext::OnPendingQueueReady() {
  if (!throttled_connections_.empty()) {
    ScheduleThrottledConnection();
  }
}

void WebTransportThrottleContext::RemovePendingHandshakes() {
  CHECK_GT(penalty_mgr_.PendingHandshakes(), 0);
  penalty_mgr_.RemovePendingHandshakes();
}

void WebTransportThrottleContext::ScheduleThrottledConnection() {
  DVLOG(1) << "WebTransportThrottleContext::ScheduleThrottledConnection() this="
           << this
           << " pending_handshakes_= " << penalty_mgr_.PendingHandshakes();

  DCHECK(!throttled_connections_.empty());

  if (penalty_mgr_.PendingHandshakes() == 0) {
    DoOnThrottleDone();
    return;
  }

  DCHECK_GT(penalty_mgr_.PendingHandshakes(), 0);

  // Don't do the calculation for large values of `pending_handshakes_` to avoid
  // integer overflow. If `pending_handshakes_` is 14, the result of the
  // calculation is 81920, so it will always get truncated to 60000.
  const int milliseconds_delay =
      penalty_mgr_.PendingHandshakes() > 13
          ? 60000
          : std::min(10 * (1 << (penalty_mgr_.PendingHandshakes() - 1)), 60000);

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
           << " pending_handshakes_= " << penalty_mgr_.PendingHandshakes()
           << " throttled_connections_.size()="
           << throttled_connections_.size();
  DCHECK(!throttled_connections_.empty());
  auto on_throttle_done = std::move(throttled_connections_.front());
  throttled_connections_.pop();
  queue_head_time_ = base::TimeTicks::Now();
  if (throttled_connections_.empty()) {
    penalty_mgr_.StopPendingQueueTimer();
  }
  auto tracker = std::make_unique<Tracker>(GetWeakPtr());
  std::move(on_throttle_done).Run(std::move(tracker));
}

void WebTransportThrottleContext::StartOneConnection() {
  DVLOG(1) << "WebTransportThrottleContext::StartOneConnection() this=" << this
           << " pending_handshakes_= " << penalty_mgr_.PendingHandshakes();

  if (throttled_connections_.empty())
    return;
  DoOnThrottleDone();
  if (!throttled_connections_.empty()) {
    ScheduleThrottledConnection();
  }
}

}  // namespace content
