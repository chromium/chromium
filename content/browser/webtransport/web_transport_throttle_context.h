// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_
#define CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_

#include <stddef.h>

#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

// Tracks a single "bucket" of pending handshakes. For frames and dedicated
// workers there is one object per Page. For shared and service workers there is
// one per profile.
class CONTENT_EXPORT WebTransportThrottleContext final
    : public base::SupportsUserData::Data {
 public:
  // Tracks an ongoing handshake. Passed to the caller of PerformThrottle(),
  // which should call one of the methods.
  class CONTENT_EXPORT Tracker final {
   public:
    // Only constructed by WebTransportThrottleContext.
    explicit Tracker(
        base::WeakPtr<WebTransportThrottleContext> throttle_context);

    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    // Destruction of the object without calling one of the methods is treated
    // like handshake failure.
    ~Tracker();

    // Records the successful end of a WebTransport handshake.
    void OnHandshakeEstablished();

    // Records a WebTransport handshake failure.
    void OnHandshakeFailed();

   private:
    base::WeakPtr<WebTransportThrottleContext> throttle_context_;
  };

  using ThrottleDoneCallback =
      base::OnceCallback<void(std::unique_ptr<Tracker>)>;

  static constexpr int kMaxPendingSessions = 64;

  enum class ThrottleResult {
    kOk,
    kTooManyPendingSessions,
  };

  WebTransportThrottleContext();
  WebTransportThrottleContext(const WebTransportThrottleContext&) = delete;
  WebTransportThrottleContext& operator=(const WebTransportThrottleContext&) =
      delete;
  ~WebTransportThrottleContext() override;

  // Attempts to start a new handshake. Returns kTooManyPendingSessions if there
  // are already too many pending handshakes. Otherwise, runs the
  // `on_throttle_done` closure, possibly synchronously, and returns kOk.
  // Immediately before the `on_throttle_done` closure is called the handshake
  // is considered started.
  ThrottleResult PerformThrottle(ThrottleDoneCallback on_throttle_done);

  // Called when a handshake fails. Adds handshake delays unless it is
  // explicitly suppressed.
  void MaybeQueueHandshakeFailurePenalty();

  void OnPendingQueueReady();

  base::WeakPtr<WebTransportThrottleContext> GetWeakPtr();

 private:
  class PenaltyManager final {
   public:
    explicit PenaltyManager(WebTransportThrottleContext*);
    PenaltyManager(const PenaltyManager&) = delete;
    PenaltyManager& operator=(const PenaltyManager&) = delete;
    ~PenaltyManager();

    // Queues a pending handshake to be considered complete after `after`.
    void QueuePending(base::TimeDelta after);

    // If there are handshakes in `pending_queue_` that can now be considered
    // finished, remove them and decrement `pending_handshakes_`. Recalculates
    // the delay for the head of `throttled_connections_` and may trigger it to
    // start as a side-effect.
    void MaybeDecrementPending();

    // Start the timer based on the `pending_queue` top.
    void ProcessPendingQueue();

    void StopPendingQueueTimer();

    int PendingHandshakes() const { return pending_handshakes_; }
    void AddPendingHandshakes() { ++pending_handshakes_; }
    void RemovePendingHandshakes() { --pending_handshakes_; }
    bool PendingQueueTimerIsRunning() {
      return pending_queue_timer_.IsRunning();
    }
    bool PendingQueueIsEmpty() { return pending_queue_.empty(); }

   private:
    // Start the timer for removing items from `pending_queue_timer_`, to fire
    // after `after` has passed.
    void StartPendingQueueTimer(base::TimeDelta after);

    const raw_ptr<WebTransportThrottleContext> throttle_context_;

    int pending_handshakes_ = 0;

    // Sessions for which the handshake has completed but we are still counting
    // as "pending" for the purposes of throttling. An items is added to this
    // queue when the handshake completes, and removed when the timer expires.
    // The "top" of the queue is the timer that will expire first.
    std::priority_queue<base::TimeTicks,
                        std::vector<base::TimeTicks>,
                        std::greater<>>
        pending_queue_;

    // A timer that will fire the next time an entry should be removed from
    // `pending_queue_`. The timer doesn't run when `throttled_connections_` is
    // empty.
    base::OneShotTimer pending_queue_timer_;
  };

  // Starts a connection immediately if there are none pending, or sets a timer
  // to start one later.
  void ScheduleThrottledConnection();

  // Calls the throttle done callback on the connection at the head of
  // `throttled_connections_`.
  void DoOnThrottleDone();

  // Starts a connection if there is one waiting, and schedules the timer for
  // the next connection.
  void StartOneConnection();

  // False when the `--webtransport-developer-mode` flag is specified, true
  // otherwise.
  const bool should_queue_handshake_failure_penalty_;

  PenaltyManager penalty_mgr_{this};

  base::queue<ThrottleDoneCallback> throttled_connections_;

  // The time that `throttled_connections_[0]` reached the front of the queue.
  // This is needed if it gets recheduled by ScheduleThrottledConnection().
  base::TimeTicks queue_head_time_;

  // A timer that will fire the next time a throttled connection should be
  // allowed to proceed. This is a reset when pending_handshakes_ is
  // decremented.
  base::OneShotTimer throttled_connections_timer_;

  base::WeakPtrFactory<WebTransportThrottleContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_
