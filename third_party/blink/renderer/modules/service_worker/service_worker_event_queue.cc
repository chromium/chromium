// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_event_queue.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// This feature flag enables a new behavior that waits
// processing events until the top-level script is evaluated.
// See: https://crbug.com/1462568
BASE_FEATURE(kServiceWorkerEventQueueWaitForScriptEvaluation,
             "ServiceWorkerEventQueueWaitForScriptEvaluation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
constexpr base::TimeDelta ServiceWorkerEventQueue::kEventTimeout;
constexpr base::TimeDelta ServiceWorkerEventQueue::kUpdateInterval;

ServiceWorkerEventQueue::StayAwakeToken::StayAwakeToken(
    base::WeakPtr<ServiceWorkerEventQueue> event_queue)
    : event_queue_(std::move(event_queue)) {
  DCHECK(event_queue_);
  event_queue_->ResetIdleTimeout();
  event_queue_->num_of_stay_awake_tokens_++;
}

ServiceWorkerEventQueue::StayAwakeToken::~StayAwakeToken() {
  // If |event_queue_| has already been destroyed, it means the worker thread
  // has already been killed.
  if (!event_queue_)
    return;
  DCHECK_GT(event_queue_->num_of_stay_awake_tokens_, 0);
  event_queue_->num_of_stay_awake_tokens_--;

  if (!event_queue_->HasInflightEvent())
    event_queue_->OnNoInflightEvent();
}

ServiceWorkerEventQueue::ServiceWorkerEventQueue(
    BeforeStartEventCallback before_start_event_callback,
    base::RepeatingClosure idle_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ServiceWorkerEventQueue(std::move(before_start_event_callback),
                              std::move(idle_callback),
                              std::move(task_runner),
                              base::DefaultTickClock::GetInstance()) {}

ServiceWorkerEventQueue::ServiceWorkerEventQueue(
    BeforeStartEventCallback before_start_event_callback,
    base::RepeatingClosure idle_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock)
    : task_runner_(std::move(task_runner)),
      before_start_event_callback_(std::move(before_start_event_callback)),
      idle_callback_(std::move(idle_callback)),
      tick_clock_(tick_clock) {
  if (!base::FeatureList::IsEnabled(
          kServiceWorkerEventQueueWaitForScriptEvaluation)) {
    is_ready_for_processing_events_ = true;
  }
}

ServiceWorkerEventQueue::~ServiceWorkerEventQueue() {
  // Abort all callbacks.
  for (auto& event : all_events_) {
    std::move(event.value->abort_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
  }
}

void ServiceWorkerEventQueue::Start() {
  DCHECK(!timer_.IsRunning());
  timer_.Start(FROM_HERE, kUpdateInterval,
               WTF::BindRepeating(&ServiceWorkerEventQueue::UpdateStatus,
                                  WTF::Unretained(this)));
  if (base::FeatureList::IsEnabled(
          kServiceWorkerEventQueueWaitForScriptEvaluation)) {
    is_ready_for_processing_events_ = true;
    ResetIdleTimeout();
    ProcessEvents();
  } else if (!HasInflightEvent() && !HasScheduledIdleCallback()) {
    // If no event happens until Start(), the idle callback should be scheduled.
    OnNoInflightEvent();
  }
}

void ServiceWorkerEventQueue::EnqueueNormal(
    int event_id,
    StartCallback start_callback,
    AbortCallback abort_callback,
    std::optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<Event>(
      event_id, Event::Type::Normal, std::move(start_callback),
      std::move(abort_callback), std::move(custom_timeout)));
}

void ServiceWorkerEventQueue::EnqueuePending(
    int event_id,
    StartCallback start_callback,
    AbortCallback abort_callback,
    std::optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<Event>(
      event_id, Event::Type::Pending, std::move(start_callback),
      std::move(abort_callback), std::move(custom_timeout)));
}

void ServiceWorkerEventQueue::EnqueueOffline(
    int event_id,
    StartCallback start_callback,
    AbortCallback abort_callback,
    std::optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<ServiceWorkerEventQueue::Event>(
      event_id, ServiceWorkerEventQueue::Event::Type::Offline,
      std::move(start_callback), std::move(abort_callback),
      std::move(custom_timeout)));
}

bool ServiceWorkerEventQueue::CanStartEvent(const Event& event) const {
  if (running_event_type_ == RunningEventType::kNone) {
    DCHECK(!HasInflightEvent());
    return true;
  }
  if (event.type == Event::Type::Offline)
    return running_event_type_ == RunningEventType::kOffline;
  return running_event_type_ == RunningEventType::kOnline;
}

std::map<int, std::unique_ptr<ServiceWorkerEventQueue::Event>>&
ServiceWorkerEventQueue::GetActiveEventQueue() {
  if (running_event_type_ == RunningEventType::kNone) {
    // Either online events or offline events can be started when inflight
    // events don't exist. If online events exist in the queue, prioritize
    // online events.
    return queued_online_events_.empty() ? queued_offline_events_
                                         : queued_online_events_;
  }
  if (running_event_type_ == RunningEventType::kOffline)
    return queued_offline_events_;
  return queued_online_events_;
}

void ServiceWorkerEventQueue::EnqueueEvent(std::unique_ptr<Event> event) {
  DCHECK(event->type != Event::Type::Pending || did_idle_timeout());
  DCHECK(!HasEvent(event->event_id));
  DCHECK(!HasEventInQueue(event->event_id));

  bool can_start_processing_events = is_ready_for_processing_events_ &&
                                     !processing_events_ &&
                                     event->type != Event::Type::Pending;

  // Start counting the timer when an event is enqueued.
  all_events_.insert(
      event->event_id,
      std::make_unique<EventInfo>(
          tick_clock_->NowTicks() +
              event->custom_timeout.value_or(kEventTimeout),
          WTF::BindOnce(std::move(event->abort_callback), event->event_id)));

  auto& queue = event->type == Event::Type::Offline ? queued_offline_events_
                                                    : queued_online_events_;
  queue.emplace(event->event_id, std::move(event));

  if (!can_start_processing_events)
    return;

  ResetIdleTimeout();
  ProcessEvents();
}

void ServiceWorkerEventQueue::ProcessEvents() {
  // TODO(crbug.com/1462568): Switch to CHECK once we resolve the bug.
  DCHECK(is_ready_for_processing_events_);
  DCHECK(!processing_events_);
  processing_events_ = true;
  auto& queue = GetActiveEventQueue();
  while (!queue.empty() && CanStartEvent(*queue.begin()->second)) {
    int event_id = queue.begin()->first;
    std::unique_ptr<Event> event = std::move(queue.begin()->second);
    queue.erase(queue.begin());
    StartEvent(event_id, std::move(event));
  }
  processing_events_ = false;

  // We have to check HasInflightEvent() and may trigger
  // OnNoInflightEvent() here because StartEvent() can call EndEvent()
  // synchronously, and EndEvent() never triggers OnNoInflightEvent()
  // while ProcessEvents() is running.
  if (!HasInflightEvent())
    OnNoInflightEvent();
}

void ServiceWorkerEventQueue::StartEvent(int event_id,
                                         std::unique_ptr<Event> event) {
  DCHECK(HasEvent(event_id));
  running_event_type_ = event->type == Event::Type::Offline
                            ? RunningEventType::kOffline
                            : RunningEventType::kOnline;
  if (before_start_event_callback_)
    before_start_event_callback_.Run(event->type == Event::Type::Offline);
  std::move(event->start_callback).Run(event_id);
}

void ServiceWorkerEventQueue::EndEvent(int event_id) {
  DCHECK(HasEvent(event_id));
  all_events_.erase(event_id);
  // Check |processing_events_| here because EndEvent() can be called
  // synchronously in StartEvent(). We don't want to trigger
  // OnNoInflightEvent() while ProcessEvents() is running.
  if (!processing_events_ && !HasInflightEvent())
    OnNoInflightEvent();
}

bool ServiceWorkerEventQueue::HasEvent(int event_id) const {
  return base::Contains(all_events_, event_id);
}

bool ServiceWorkerEventQueue::HasEventInQueue(int event_id) const {
  return (base::Contains(queued_online_events_, event_id) ||
          base::Contains(queued_offline_events_, event_id));
}

std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken>
ServiceWorkerEventQueue::CreateStayAwakeToken() {
  return std::make_unique<ServiceWorkerEventQueue::StayAwakeToken>(
      weak_factory_.GetWeakPtr());
}

void ServiceWorkerEventQueue::SetIdleDelay(base::TimeDelta idle_delay) {
  idle_delay_ = idle_delay;

  if (HasInflightEvent())
    return;

  if (did_idle_timeout()) {
    // The idle callback has already been called. It should not be called again
    // until this worker becomes active.
    return;
  }

  // There should be a scheduled idle callback because this is now in the idle
  // delay. The idle callback will be rescheduled based on the new idle delay.
  DCHECK(HasScheduledIdleCallback());
  idle_callback_handle_.Cancel();

  // Calculate the updated time of when the |idle_callback_| should be invoked.
  DCHECK(!last_no_inflight_event_time_.is_null());
  auto new_idle_callback_time = last_no_inflight_event_time_ + idle_delay;
  base::TimeDelta delta_until_idle =
      new_idle_callback_time - tick_clock_->NowTicks();

  if (delta_until_idle <= base::Seconds(0)) {
    // The new idle delay is shorter than the previous idle delay, and the idle
    // time has been already passed. Let's run the idle callback immediately.
    TriggerIdleCallback();
    return;
  }

  // Let's schedule the idle callback in |delta_until_idle|.
  ScheduleIdleCallback(delta_until_idle);
}

void ServiceWorkerEventQueue::CheckEventQueue() {
  if (!HasInflightEvent()) {
    OnNoInflightEvent();
  }
}

void ServiceWorkerEventQueue::UpdateStatus() {
  base::TimeTicks now = tick_clock_->NowTicks();

  // Construct a new map because WTF::HashMap doesn't support deleting elements
  // while iterating.
  HashMap<int /* event_id */, std::unique_ptr<EventInfo>> new_all_events;

  bool should_idle_delay_to_be_zero = false;

  // Time out all events exceeding `kEventTimeout`.
  for (auto& it : all_events_) {
    // Check if the event has timed out.
    int event_id = it.key;
    std::unique_ptr<EventInfo>& event_info = it.value;
    if (event_info->expiration_time > now) {
      new_all_events.insert(event_id, std::move(event_info));
      continue;
    }

    // The event may still be in one of the queues when it timed out. Try to
    // remove the event from both.
    queued_online_events_.erase(event_id);
    queued_offline_events_.erase(event_id);

    // Run the abort callback.
    std::move(event_info->abort_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::TIMEOUT);

    should_idle_delay_to_be_zero = true;
  }
  all_events_.swap(new_all_events);

  // Set idle delay to zero if needed.
  if (should_idle_delay_to_be_zero) {
    // Inflight events might be timed out and there might be no inflight event
    // at this point.
    if (!HasInflightEvent()) {
      OnNoInflightEvent();
    }
    // Shut down the worker as soon as possible since the worker may have gone
    // into bad state.
    SetIdleDelay(base::Seconds(0));
  }
}

void ServiceWorkerEventQueue::ScheduleIdleCallback(base::TimeDelta delay) {
  DCHECK(!HasInflightEvent());
  DCHECK(!HasScheduledIdleCallback());

  // WTF::Unretained() is safe because the task runner will be destroyed
  // before |this| is destroyed at ServiceWorkerGlobalScope::Dispose().
  idle_callback_handle_ = PostDelayedCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::BindOnce(&ServiceWorkerEventQueue::TriggerIdleCallback,
                    WTF::Unretained(this)),
      delay);
}

void ServiceWorkerEventQueue::TriggerIdleCallback() {
  DCHECK(!HasInflightEvent());
  DCHECK(!HasScheduledIdleCallback());
  DCHECK(!did_idle_timeout_);

  did_idle_timeout_ = true;
  idle_callback_.Run();
}

void ServiceWorkerEventQueue::OnNoInflightEvent() {
  DCHECK(!HasInflightEvent());
  running_event_type_ = RunningEventType::kNone;
  // There might be events in the queue because offline (or non-offline) events
  // can be enqueued during running non-offline (or offline) events.
  auto& queue = GetActiveEventQueue();
  if (!queue.empty()) {
    ProcessEvents();
    return;
  }
  last_no_inflight_event_time_ = tick_clock_->NowTicks();
  ScheduleIdleCallback(idle_delay_);
}

bool ServiceWorkerEventQueue::HasInflightEvent() const {
  size_t num_queued_events =
      queued_online_events_.size() + queued_offline_events_.size();
  DCHECK_LE(num_queued_events, all_events_.size());
  return all_events_.size() - num_queued_events > 0 ||
         num_of_stay_awake_tokens_ > 0;
}

void ServiceWorkerEventQueue::ResetIdleTimeout() {
  last_no_inflight_event_time_ = base::TimeTicks();
  idle_callback_handle_.Cancel();
  did_idle_timeout_ = false;
}

bool ServiceWorkerEventQueue::HasScheduledIdleCallback() const {
  return idle_callback_handle_.IsActive();
}

int ServiceWorkerEventQueue::NextEventId() {
  CHECK_LT(next_event_id_, std::numeric_limits<int>::max());
  return next_event_id_++;
}

ServiceWorkerEventQueue::Event::Event(
    int event_id,
    ServiceWorkerEventQueue::Event::Type type,
    StartCallback start_callback,
    AbortCallback abort_callback,
    std::optional<base::TimeDelta> custom_timeout)
    : event_id(event_id),
      type(type),
      start_callback(std::move(start_callback)),
      abort_callback(std::move(abort_callback)),
      custom_timeout(custom_timeout) {}

ServiceWorkerEventQueue::Event::~Event() = default;

ServiceWorkerEventQueue::EventInfo::EventInfo(
    base::TimeTicks expiration_time,
    base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)>
        abort_callback)
    : expiration_time(expiration_time),
      abort_callback(std::move(abort_callback)) {}

ServiceWorkerEventQueue::EventInfo::~EventInfo() = default;

}  // namespace blink
