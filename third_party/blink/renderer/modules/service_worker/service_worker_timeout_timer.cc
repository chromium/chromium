// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_timeout_timer.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

int NextEventId() {
  // Event id should not start from zero since HashMap in Blink requires
  // non-zero keys.
  static base::AtomicSequenceNumber s_event_id_sequence;
  int next_event_id = s_event_id_sequence.GetNext() + 1;
  CHECK_LT(next_event_id, std::numeric_limits<int>::max());
  return next_event_id;
}

}  // namespace

// static
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kIdleDelay;
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kEventTimeout;
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kUpdateInterval;

ServiceWorkerTimeoutTimer::StayAwakeToken::StayAwakeToken(
    base::WeakPtr<ServiceWorkerTimeoutTimer> timer)
    : timer_(std::move(timer)) {
  DCHECK(timer_);
  timer_->num_of_stay_awake_tokens_++;
}

ServiceWorkerTimeoutTimer::StayAwakeToken::~StayAwakeToken() {
  // If |timer_| has already been destroyed, it means the worker thread has
  // already been killed.
  if (!timer_)
    return;
  DCHECK_GT(timer_->num_of_stay_awake_tokens_, 0);
  timer_->num_of_stay_awake_tokens_--;

  if (!timer_->HasInflightEvent())
    timer_->OnNoInflightEvent();
}

ServiceWorkerTimeoutTimer::ServiceWorkerTimeoutTimer(
    base::RepeatingClosure idle_callback)
    : ServiceWorkerTimeoutTimer(std::move(idle_callback),
                                base::DefaultTickClock::GetInstance()) {}

ServiceWorkerTimeoutTimer::ServiceWorkerTimeoutTimer(
    base::RepeatingClosure idle_callback,
    const base::TickClock* tick_clock)
    : idle_callback_(std::move(idle_callback)), tick_clock_(tick_clock) {}

ServiceWorkerTimeoutTimer::~ServiceWorkerTimeoutTimer() {
  in_dtor_ = true;
  // Abort all callbacks.
  for (auto& event : inflight_events_) {
    std::move(event.abort_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
  }
}

void ServiceWorkerTimeoutTimer::Start() {
  DCHECK(!timer_.IsRunning());
  // |idle_callback_| will be invoked if no event happens in |kIdleDelay|.
  if (!HasInflightEvent() && idle_time_.is_null())
    idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
  timer_.Start(FROM_HERE, kUpdateInterval,
               WTF::BindRepeating(&ServiceWorkerTimeoutTimer::UpdateStatus,
                                  WTF::Unretained(this)));
}

int ServiceWorkerTimeoutTimer::StartEvent(AbortCallback abort_callback) {
  return StartEventWithCustomTimeout(std::move(abort_callback), kEventTimeout);
}

int ServiceWorkerTimeoutTimer::StartEventWithCustomTimeout(
    AbortCallback abort_callback,
    base::TimeDelta timeout) {
  if (did_idle_timeout()) {
    DCHECK(!running_pending_tasks_);
    idle_time_ = base::TimeTicks();
    did_idle_timeout_ = false;

    running_pending_tasks_ = true;
    while (!pending_tasks_.IsEmpty()) {
      pending_tasks_.TakeFirst().Run();
    }
    running_pending_tasks_ = false;
  }

  idle_time_ = base::TimeTicks();
  const int event_id = NextEventId();
  std::set<EventInfo>::iterator iter;
  bool is_inserted;
  std::tie(iter, is_inserted) =
      inflight_events_.emplace(event_id, tick_clock_->NowTicks() + timeout,
                               WTF::Bind(std::move(abort_callback), event_id));
  DCHECK(is_inserted);
  id_event_map_.insert(event_id, iter);
  return event_id;
}

void ServiceWorkerTimeoutTimer::EndEvent(int event_id) {
  DCHECK(HasEvent(event_id));

  auto iter = id_event_map_.find(event_id);
  inflight_events_.erase(iter->value);
  id_event_map_.erase(iter);

  if (!HasInflightEvent())
    OnNoInflightEvent();
}

bool ServiceWorkerTimeoutTimer::HasEvent(int event_id) const {
  return id_event_map_.find(event_id) != id_event_map_.end();
}

std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken>
ServiceWorkerTimeoutTimer::CreateStayAwakeToken() {
  return std::make_unique<ServiceWorkerTimeoutTimer::StayAwakeToken>(
      weak_factory_.GetWeakPtr());
}

void ServiceWorkerTimeoutTimer::PushPendingTask(
    base::OnceClosure pending_task) {
  DCHECK(did_idle_timeout());
  pending_tasks_.emplace_back(std::move(pending_task));
}

void ServiceWorkerTimeoutTimer::SetIdleTimerDelayToZero() {
  zero_idle_timer_delay_ = true;
  if (!HasInflightEvent())
    MaybeTriggerIdleTimer();
}

void ServiceWorkerTimeoutTimer::UpdateStatus() {
  base::TimeTicks now = tick_clock_->NowTicks();

  // Abort all events exceeding |kEventTimeout|.
  auto iter = inflight_events_.begin();
  while (iter != inflight_events_.end() && iter->expiration_time <= now) {
    int event_id = iter->id;
    base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)> callback =
        std::move(iter->abort_callback);
    iter = inflight_events_.erase(iter);
    id_event_map_.erase(event_id);
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::TIMEOUT);
    // Shut down the worker as soon as possible since the worker may have gone
    // into bad state.
    zero_idle_timer_delay_ = true;
  }

  // If the worker is now idle, set the |idle_time_| and possibly trigger the
  // idle callback.
  if (!HasInflightEvent() && idle_time_.is_null()) {
    OnNoInflightEvent();
    return;
  }

  if (!idle_time_.is_null() && idle_time_ < now) {
    did_idle_timeout_ = true;
    idle_callback_.Run();
  }
}

bool ServiceWorkerTimeoutTimer::MaybeTriggerIdleTimer() {
  DCHECK(!HasInflightEvent());
  if (!zero_idle_timer_delay_)
    return false;

  did_idle_timeout_ = true;
  idle_callback_.Run();
  return true;
}

void ServiceWorkerTimeoutTimer::OnNoInflightEvent() {
  DCHECK(!HasInflightEvent());
  idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
  MaybeTriggerIdleTimer();
}

bool ServiceWorkerTimeoutTimer::HasInflightEvent() const {
  return !inflight_events_.empty() || running_pending_tasks_ ||
         num_of_stay_awake_tokens_ > 0;
}

ServiceWorkerTimeoutTimer::EventInfo::EventInfo(
    int id,
    base::TimeTicks expiration_time,
    base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)>
        abort_callback)
    : id(id),
      expiration_time(expiration_time),
      abort_callback(std::move(abort_callback)) {}

ServiceWorkerTimeoutTimer::EventInfo::~EventInfo() = default;

bool ServiceWorkerTimeoutTimer::EventInfo::operator<(
    const EventInfo& other) const {
  if (expiration_time == other.expiration_time)
    return id < other.id;
  return expiration_time < other.expiration_time;
}

}  // namespace blink
