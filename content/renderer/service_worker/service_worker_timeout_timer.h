// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_

#include <map>
#include <set>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace base {

class TickClock;

}  // namespace base

namespace content {

// ServiceWorkerTimeoutTimer manages two types of timeouts: the long standing
// event timeout and the idle timeout.
//
// S13nServiceWorker:
// 1) Event timeout: when an event starts, StartEvent() records the expiration
// time of the event (kEventTimeout). If EndEvent() has not been called within
// the timeout time, |abort_callback| passed to StartEvent() is called. Also,
// |zero_idle_timer_delay_| is set to true to shut down the worker as soon as
// possible since the worker may have gone into bad state.
// 2) Idle timeout: when a certain time has passed (kIdleDelay) since all of
// events have ended, ServiceWorkerTimeoutTimer calls the |idle_callback|.
// |idle_callback| will be continuously called at a certain interval
// (kUpdateInterval) until the next event starts.
//
// The lifetime of ServiceWorkerTimeoutTimer is the same with the worker
// thread. If ServiceWorkerTimeoutTimer is destructed while there are inflight
// events, all |abort_callback|s will be immediately called.
//
// Non-S13nServiceWorker:
// Does nothing except calls the abort callbacks upon destruction.
class CONTENT_EXPORT ServiceWorkerTimeoutTimer {
 public:
  // A token to keep the timeout timer from going into the idle state if any of
  // them are alive.
  class CONTENT_EXPORT StayAwakeToken {
   public:
    explicit StayAwakeToken(base::WeakPtr<ServiceWorkerTimeoutTimer> timer);
    ~StayAwakeToken();

   private:
    base::WeakPtr<ServiceWorkerTimeoutTimer> timer_;
  };

  explicit ServiceWorkerTimeoutTimer(base::RepeatingClosure idle_callback);
  // For testing.
  ServiceWorkerTimeoutTimer(base::RepeatingClosure idle_callback,
                            const base::TickClock* tick_clock);
  ~ServiceWorkerTimeoutTimer();

  // StartEvent() should be called at the beginning of an event. It returns an
  // event id, which is unique among threads in the same process.
  // The event id should be passed to EndEvent() when the event has finished.
  // If there are pending tasks queued by PushPendingTask(), they will
  // run in order synchronouslly in StartEvent().
  // See the class comment to know when |abort_callback| runs.
  int StartEvent(base::OnceCallback<void(int /* event_id */)> abort_callback);
  // This is basically the same as StartEvent, but you can customize the
  // timeout time until |abort_callback| runs by |timeout|.
  int StartEventWithCustomTimeout(
      base::OnceCallback<void(int /* event_id */)> abort_callback,
      base::TimeDelta timeout);
  void EndEvent(int event_id);

  // Creates a StayAwakeToken to ensure that the idle timer won't be triggered
  // while any of these are alive.
  std::unique_ptr<StayAwakeToken> CreateStayAwakeToken();

  // Pushes a task which is expected to run after any event starts again to a
  // pending task queue. The tasks will run at the next StartEvent() call.
  // PushPendingTask() should be called if the idle timeout occurred
  // (did_idle_timeout() returns true).
  void PushPendingTask(base::OnceClosure pending_task);

  // Sets the |zero_idle_timer_delay_| to true and triggers the idle callback if
  // there are not inflight events. If there are, the callback will be called
  // next time when the set of inflight events becomes empty in EndEvent().
  void SetIdleTimerDelayToZero();

  // Returns true if the timer thinks no events ran for a while, and has
  // triggered the |idle_callback| passed to the constructor. It'll be reset to
  // false again when StartEvent() is called.
  bool did_idle_timeout() const { return did_idle_timeout_; }

  // Idle timeout duration since the last event has finished.
  static constexpr base::TimeDelta kIdleDelay =
      base::TimeDelta::FromSeconds(30);
  // Duration of the long standing event timeout since StartEvent() has been
  // called.
  static constexpr base::TimeDelta kEventTimeout =
      base::TimeDelta::FromMinutes(5);
  // ServiceWorkerTimeoutTimer periodically updates the timeout state by
  // kUpdateInterval.
  static constexpr base::TimeDelta kUpdateInterval =
      base::TimeDelta::FromSeconds(30);

 private:
  // Updates the internal states and fires timeout callbacks if any.
  void UpdateStatus();

  // Triggers idle timer if |zero_idle_timer_delay_| is true. Returns true if
  // the idle callback is called.
  bool MaybeTriggerIdleTimer();

  // Sets the |idle_time_| and maybe calls |idle_callback_| immediately if the
  // timeout delay is set to zero.
  void OnNoInflightEvent();

  // Returns true if there are running events.
  bool HasInflightEvent() const;

  struct EventInfo {
    EventInfo(int id,
              base::TimeTicks expiration_time,
              base::OnceClosure abort_callback);
    ~EventInfo();
    // Compares |expiration_time|, or |id| if |expiration_time| is the same.
    bool operator<(const EventInfo& other) const;

    const int id;
    const base::TimeTicks expiration_time;
    mutable base::OnceClosure abort_callback;
  };

  // For long standing event timeouts. Ordered by expiration time.
  std::set<EventInfo> inflight_events_;

  // For long standing event timeouts. This is used to look up an event in
  // |inflight_events_| by its id.
  std::map<int /* event_id */, std::set<EventInfo>::iterator> id_event_map_;

  // For idle timeouts. The time the service worker started being considered
  // idle. This time is null if there are any inflight events.
  base::TimeTicks idle_time_;

  // Set to true if the idle callback should be fired immediately after all
  // inflight events finish.
  bool zero_idle_timer_delay_ = false;

  // For idle timeouts. Invoked when UpdateStatus() is called after
  // |idle_time_|.
  base::RepeatingClosure idle_callback_;

  // Set to true once |idle_callback_| has been invoked. Set to false when
  // StartEvent() is called.
  bool did_idle_timeout_ = false;

  // Tasks that are to be run after the next StartEvent() call. In practice, the
  // caller adds pending tasks after the service worker requested the browser to
  // terminate it due to idleness. These tasks run once StartEvent() is called
  // due to a new event from the browser, signalling that the browser decided
  // not to terminate the worker.
  base::queue<base::OnceClosure> pending_tasks_;

  // Set to true during running |pending_tasks_|. This is used for avoiding to
  // invoke |idle_callback_| when running |pending_tasks_|.
  bool running_pending_tasks_ = false;

  // The number of the living StayAwakeToken. See also class comments.
  int num_of_stay_awake_tokens_ = 0;

  // |timer_| invokes UpdateEventStatus() periodically.
  base::RepeatingTimer timer_;

  // |tick_clock_| outlives |this|.
  const base::TickClock* const tick_clock_;

  base::WeakPtrFactory<ServiceWorkerTimeoutTimer> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_
