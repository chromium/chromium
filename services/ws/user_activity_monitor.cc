// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/user_activity_monitor.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"
#include "ui/aura/env.h"

namespace ws {

UserActivityMonitor::UserActivityMonitor(
    aura::Env* env,
    std::unique_ptr<const base::TickClock> clock)
    : env_(env), now_clock_(std::move(clock)) {
  if (!now_clock_)
    now_clock_ = std::make_unique<base::DefaultTickClock>();
  last_activity_ = now_clock_->NowTicks();

  env_->AddPreTargetHandler(this);
}

UserActivityMonitor::~UserActivityMonitor() {
  env_->RemovePreTargetHandler(this);
}

void UserActivityMonitor::AddBinding(
    mojom::UserActivityMonitorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void UserActivityMonitor::AddUserActivityObserver(
    uint32_t delay_between_notify_secs,
    mojom::UserActivityObserverPtr observer) {
  ActivityObserverInfo info;
  info.delay = base::TimeDelta::FromSeconds(delay_between_notify_secs);
  observer.set_connection_error_handler(
      base::BindRepeating(&UserActivityMonitor::OnActivityObserverDisconnected,
                          base::Unretained(this), observer.get()));
  activity_observers_.push_back(std::make_pair(info, std::move(observer)));
}

void UserActivityMonitor::AddUserIdleObserver(
    uint32_t idleness_in_minutes,
    mojom::UserIdleObserverPtr observer) {
  IdleObserverInfo info;
  info.idle_duration = base::TimeDelta::FromMinutes(idleness_in_minutes);
  base::TimeTicks now = now_clock_->NowTicks();
  DCHECK(!last_activity_.is_null());
  bool user_is_active = (now - last_activity_ < info.idle_duration);
  info.idle_state = user_is_active ? mojom::UserIdleObserver::IdleState::ACTIVE
                                   : mojom::UserIdleObserver::IdleState::IDLE;
  info.last_idle_state_notification = now;
  observer->OnUserIdleStateChanged(info.idle_state);
  observer.set_connection_error_handler(
      base::BindRepeating(&UserActivityMonitor::OnIdleObserverDisconnected,
                          base::Unretained(this), observer.get()));
  idle_observers_.push_back(std::make_pair(info, std::move(observer)));
  if (user_is_active)
    ActivateIdleTimer();
}

void UserActivityMonitor::OnEvent(ui::Event* event) {
  base::TimeTicks now = now_clock_->NowTicks();
  for (auto& pair : activity_observers_) {
    ActivityObserverInfo* info = &(pair.first);
    if (info->last_activity_notification.is_null() ||
        (now - info->last_activity_notification) > info->delay) {
      pair.second->OnUserActivity();
      info->last_activity_notification = now;
    }
  }

  // Wake up all the 'idle' observers.
  for (auto& pair : idle_observers_) {
    IdleObserverInfo* info = &(pair.first);
    if (info->idle_state == mojom::UserIdleObserver::IdleState::ACTIVE)
      continue;
    info->last_idle_state_notification = now;
    info->idle_state = mojom::UserIdleObserver::IdleState::ACTIVE;
    pair.second->OnUserIdleStateChanged(info->idle_state);
  }

  last_activity_ = now;

  // Restart the timer if there are some idle observers.
  if (idle_timer_.IsRunning())
    idle_timer_.Reset();
}

void UserActivityMonitor::ActivateIdleTimer() {
  if (idle_timer_.IsRunning())
    return;
  idle_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(1), this,
                    &UserActivityMonitor::OnMinuteTimer);
}

void UserActivityMonitor::OnMinuteTimer() {
  base::TimeTicks now = now_clock_->NowTicks();
  bool active_observer = false;
  for (auto& pair : idle_observers_) {
    IdleObserverInfo* info = &(pair.first);
    if (info->idle_state == mojom::UserIdleObserver::IdleState::IDLE)
      continue;
    if (now - info->last_idle_state_notification < info->idle_duration) {
      active_observer = true;
      continue;
    }
    info->last_idle_state_notification = now;
    info->idle_state = mojom::UserIdleObserver::IdleState::IDLE;
    pair.second->OnUserIdleStateChanged(info->idle_state);
  }
  // All observers are already notified of IDLE. No point running the timer
  // anymore.
  if (!active_observer)
    idle_timer_.Stop();
}

void UserActivityMonitor::OnActivityObserverDisconnected(
    mojom::UserActivityObserver* observer) {
  activity_observers_.erase(std::remove_if(
      activity_observers_.begin(), activity_observers_.end(),
      [observer](
          const std::pair<ActivityObserverInfo, mojom::UserActivityObserverPtr>&
              pair) { return pair.second.get() == observer; }));
}

void UserActivityMonitor::OnIdleObserverDisconnected(
    mojom::UserIdleObserver* observer) {
  idle_observers_.erase(std::remove_if(
      idle_observers_.begin(), idle_observers_.end(),
      [observer](
          const std::pair<IdleObserverInfo, mojom::UserIdleObserverPtr>& pair) {
        return pair.second.get() == observer;
      }));
}

}  // namespace ws
