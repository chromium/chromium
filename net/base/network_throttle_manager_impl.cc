// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_throttle_manager_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"

namespace net {

const size_t NetworkThrottleManagerImpl::kActiveRequestThrottlingLimit = 2;
const int NetworkThrottleManagerImpl::kMedianLifetimeMultiple = 5;

// Initial estimate based on the median in the
// Net.RequestTime2.Success histogram, excluding cached results by eye.
const int NetworkThrottleManagerImpl::kInitialMedianInMs = 400;

// Set timers slightly further into the future than they need to be set, so
// that the algorithm isn't vulnerable to timer round off errors triggering
// the callback before the throttle would be considered aged out of the set.
// Set to 17 to hanlde systems with |!base::TimeTicks::IsHighResolution()|.
// Note that even if the timer goes off before it should, all that should cost
// is a second task; this class does not rely on timer accuracy for its
// correctness.
const int kTimerFudgeInMs = 17;

class NetworkThrottleManagerImpl::ThrottleImpl
    : public NetworkThrottleManager::Throttle {
 public:
  // Allowed state transitions are BLOCKED -> OUTSTANDING -> AGED.
  // Throttles may be created in the BLOCKED or OUTSTANDING states.
  enum class State {
    // Not allowed to proceed by manager.
    BLOCKED,

    // Allowed to proceed, counts as an "outstanding" request for
    // manager accounting purposes.
    OUTSTANDING,

    // Old enough to not count as "outstanding" anymore for
    // manager accounting purposes.
    AGED
  };

  using ThrottleListQueuePointer =
      NetworkThrottleManagerImpl::ThrottleList::iterator;

  // Caller must arrange that |*delegate| and |*manager| outlive
  // the ThrottleImpl class.
  ThrottleImpl(bool blocked,
               RequestPriority priority,
               ThrottleDelegate* delegate,
               NetworkThrottleManagerImpl* manager,
               ThrottleListQueuePointer queue_pointer);

  ~ThrottleImpl() override;

  // Throttle:
  bool IsBlocked() const override;
  RequestPriority Priority() const override;
  void SetPriority(RequestPriority priority) override;

  State state() const { return state_; }

  ThrottleListQueuePointer queue_pointer() const { return queue_pointer_; }
  void set_queue_pointer(const ThrottleListQueuePointer& pointer) {
    if (state_ != State::AGED)
      DCHECK_EQ(this, *pointer);
    queue_pointer_ = pointer;
  }

  void set_start_time(base::TimeTicks start_time) { start_time_ = start_time; }
  base::TimeTicks start_time() const { return start_time_; }

  // Change the throttle's state to AGED.  The previous
  // state must be OUTSTANDING.
  void SetAged();

  // Note that this call calls the delegate, and hence may result in
  // re-entrant calls into the manager or ThrottleImpl.  The manager should
  // not rely on any state other than its own existence being persistent
  // across this call.
  void NotifyUnblocked();

 private:
  State state_;
  RequestPriority priority_;
  ThrottleDelegate* const delegate_;
  NetworkThrottleManagerImpl* const manager_;

  base::TimeTicks start_time_;

  // To allow deletion from the blocked queue (when the throttle is in the
  // blocked queue).
  ThrottleListQueuePointer queue_pointer_;

  DISALLOW_COPY_AND_ASSIGN(ThrottleImpl);
};

NetworkThrottleManagerImpl::ThrottleImpl::ThrottleImpl(
    bool blocked,
    RequestPriority priority,
    NetworkThrottleManager::ThrottleDelegate* delegate,
    NetworkThrottleManagerImpl* manager,
    ThrottleListQueuePointer queue_pointer)
    : state_(blocked ? State::BLOCKED : State::OUTSTANDING),
      priority_(priority),
      delegate_(delegate),
      manager_(manager),
      queue_pointer_(queue_pointer) {
  DCHECK(delegate);
  if (!blocked)
    start_time_ = manager->tick_clock_->NowTicks();
}

NetworkThrottleManagerImpl::ThrottleImpl::~ThrottleImpl() {
  manager_->OnThrottleDestroyed(this);
}

bool NetworkThrottleManagerImpl::ThrottleImpl::IsBlocked() const {
  return state_ == State::BLOCKED;
}

RequestPriority NetworkThrottleManagerImpl::ThrottleImpl::Priority() const {
  return priority_;
}

void NetworkThrottleManagerImpl::ThrottleImpl::SetPriority(
    RequestPriority new_priority) {
  RequestPriority old_priority(priority_);
  if (old_priority == new_priority)
    return;
  priority_ = new_priority;
  manager_->OnThrottlePriorityChanged(this, old_priority, new_priority);
}

void NetworkThrottleManagerImpl::ThrottleImpl::SetAged() {
  DCHECK_EQ(State::OUTSTANDING, state_);
  state_ = State::AGED;
}

void NetworkThrottleManagerImpl::ThrottleImpl::NotifyUnblocked() {
  // This method should only be called once, and only if the
  // current state is blocked.
  DCHECK_EQ(State::BLOCKED, state_);
  state_ = State::OUTSTANDING;
  delegate_->OnThrottleUnblocked(this);
}

NetworkThrottleManagerImpl::NetworkThrottleManagerImpl()
    : lifetime_median_estimate_(PercentileEstimator::kMedianPercentile,
                                kInitialMedianInMs),
      outstanding_recomputation_timer_(std::make_unique<base::OneShotTimer>()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {}

NetworkThrottleManagerImpl::~NetworkThrottleManagerImpl() = default;

std::unique_ptr<NetworkThrottleManager::Throttle>
NetworkThrottleManagerImpl::CreateThrottle(
    NetworkThrottleManager::ThrottleDelegate* delegate,
    RequestPriority priority,
    bool ignore_limits) {
  bool blocked =
      (!ignore_limits && priority == THROTTLED &&
       outstanding_throttles_.size() >= kActiveRequestThrottlingLimit);

  std::unique_ptr<NetworkThrottleManagerImpl::ThrottleImpl> throttle(
      new ThrottleImpl(blocked, priority, delegate, this,
                       blocked_throttles_.end()));

  ThrottleList& insert_list(blocked ? blocked_throttles_
                                    : outstanding_throttles_);

  throttle->set_queue_pointer(
      insert_list.insert(insert_list.end(), throttle.get()));

  // In case oustanding_throttles_ was empty, set up timer.
  if (!blocked)
    RecomputeOutstanding();

  return std::move(throttle);
}

void NetworkThrottleManagerImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  DCHECK(!outstanding_recomputation_timer_->IsRunning());
  outstanding_recomputation_timer_ =
      std::make_unique<base::OneShotTimer>(tick_clock_);
}

bool NetworkThrottleManagerImpl::ConditionallyTriggerTimerForTesting() {
  if (!outstanding_recomputation_timer_->IsRunning() ||
      (tick_clock_->NowTicks() <
       outstanding_recomputation_timer_->desired_run_time())) {
    return false;
  }

  base::Closure timer_callback(outstanding_recomputation_timer_->user_task());
  outstanding_recomputation_timer_->Stop();
  timer_callback.Run();
  return true;
}

void NetworkThrottleManagerImpl::OnThrottlePriorityChanged(
    NetworkThrottleManagerImpl::ThrottleImpl* throttle,
    RequestPriority old_priority,
    RequestPriority new_priority) {
  // The only case requiring a state change is if the priority change
  // implies unblocking, which can only happen on a transition from blocked
  // (implies THROTTLED) to non-THROTTLED.
  if (throttle->IsBlocked() && new_priority != THROTTLED) {
    // May result in re-entrant calls into this class.
    UnblockThrottle(throttle);
  }
}

void NetworkThrottleManagerImpl::OnThrottleDestroyed(ThrottleImpl* throttle) {
  switch (throttle->state()) {
    case ThrottleImpl::State::BLOCKED:
      DCHECK(throttle->queue_pointer() != blocked_throttles_.end());
      DCHECK_EQ(throttle, *(throttle->queue_pointer()));
      blocked_throttles_.erase(throttle->queue_pointer());
      break;
    case ThrottleImpl::State::OUTSTANDING:
      DCHECK(throttle->queue_pointer() != outstanding_throttles_.end());
      DCHECK_EQ(throttle, *(throttle->queue_pointer()));
      outstanding_throttles_.erase(throttle->queue_pointer());
      FALLTHROUGH;
    case ThrottleImpl::State::AGED:
      DCHECK(!throttle->start_time().is_null());
      lifetime_median_estimate_.AddSample(
          (tick_clock_->NowTicks() - throttle->start_time())
              .InMillisecondsRoundedUp());
      break;
  }

  DCHECK(!base::Contains(blocked_throttles_, throttle));
  DCHECK(!base::Contains(outstanding_throttles_, throttle));

  // Unblock the throttles if there's some chance there's a throttle to
  // unblock.
  if (outstanding_throttles_.size() < kActiveRequestThrottlingLimit &&
      !blocked_throttles_.empty()) {
    // Via PostTask so there aren't upcalls from within destructors.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkThrottleManagerImpl::MaybeUnblockThrottles,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void NetworkThrottleManagerImpl::RecomputeOutstanding() {
  // Remove all throttles that have aged out of the outstanding set.
  base::TimeTicks now(tick_clock_->NowTicks());
  base::TimeDelta age_horizon(base::TimeDelta::FromMilliseconds((
      kMedianLifetimeMultiple * lifetime_median_estimate_.current_estimate())));
  while (!outstanding_throttles_.empty()) {
    ThrottleImpl* throttle = *outstanding_throttles_.begin();
    if (throttle->start_time() + age_horizon >= now)
      break;

    outstanding_throttles_.erase(outstanding_throttles_.begin());
    throttle->SetAged();
    throttle->set_queue_pointer(outstanding_throttles_.end());
  }

  if (outstanding_throttles_.empty())
    return;

  // If the timer is already running, be conservative and leave it alone;
  // the time for which it would be set will only be later than when it's
  // currently set.
  // This addresses, e.g., situations where a RecomputeOutstanding() races
  // with a running timer which would unblock blocked throttles.
  if (outstanding_recomputation_timer_->IsRunning())
    return;

  ThrottleImpl* first_throttle(*outstanding_throttles_.begin());
  DCHECK_GE(first_throttle->start_time() + age_horizon, now);

  outstanding_recomputation_timer_->Start(
      FROM_HERE,
      ((first_throttle->start_time() + age_horizon) - now +
       base::TimeDelta::FromMilliseconds(kTimerFudgeInMs)),
      // Unretained use of |this| is safe because the timer is
      // owned by this object, and will be torn down if this object
      // is destroyed.
      base::Bind(&NetworkThrottleManagerImpl::MaybeUnblockThrottles,
                 base::Unretained(this)));
}

void NetworkThrottleManagerImpl::UnblockThrottle(ThrottleImpl* throttle) {
  DCHECK(throttle->IsBlocked());

  blocked_throttles_.erase(throttle->queue_pointer());
  throttle->set_start_time(tick_clock_->NowTicks());
  throttle->set_queue_pointer(
      outstanding_throttles_.insert(outstanding_throttles_.end(), throttle));

  // Called in case |*throttle| was added to a null set.
  RecomputeOutstanding();

  // May result in re-entrant calls into this class.
  throttle->NotifyUnblocked();
}

void NetworkThrottleManagerImpl::MaybeUnblockThrottles() {
  RecomputeOutstanding();

  while (outstanding_throttles_.size() < kActiveRequestThrottlingLimit &&
         !blocked_throttles_.empty()) {
    // NOTE: This call may result in reentrant calls into
    // NetworkThrottleManagerImpl; no state should be assumed to be
    // persistent across this call.
    UnblockThrottle(blocked_throttles_.front());
  }
}

}  // namespace net
