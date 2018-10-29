// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_monitor.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/trace_event/trace_event.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"

namespace storage {

// StorageObserverList:

StorageObserverList::ObserverState::ObserverState()
    : requires_update(false) {
}

StorageObserverList::StorageObserverList() = default;

StorageObserverList::~StorageObserverList() = default;

void StorageObserverList::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  ObserverState& observer_state = observer_state_map_[observer];
  observer_state.origin = params.filter.origin;
  observer_state.rate = params.rate;
}

void StorageObserverList::RemoveObserver(StorageObserver* observer) {
  observer_state_map_.erase(observer);
}

int StorageObserverList::ObserverCount() const {
  return observer_state_map_.size();
}

void StorageObserverList::OnStorageChange(const StorageObserver::Event& event) {
  // crbug.com/349708
  TRACE_EVENT0("io",
               "HostStorageObserversStorageObserverList::OnStorageChange");

  for (auto& observer_state_pair : observer_state_map_)
    observer_state_pair.second.requires_update = true;

  MaybeDispatchEvent(event);
}

void StorageObserverList::MaybeDispatchEvent(
    const StorageObserver::Event& event) {
  // crbug.com/349708
  TRACE_EVENT0("io", "StorageObserverList::MaybeDispatchEvent");

  notification_timer_.Stop();
  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool all_observers_notified = true;

  for (auto& observer_state_pair : observer_state_map_) {
    StorageObserver* observer = observer_state_pair.first;
    ObserverState& state = observer_state_pair.second;

    if (!state.requires_update)
      continue;

    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeDelta delta = current_time - state.last_notification_time;
    if (state.last_notification_time.is_null() || delta >= state.rate) {
      state.requires_update = false;
      state.last_notification_time = current_time;

      if (state.origin == event.filter.origin) {
        // crbug.com/349708
        TRACE_EVENT0("io",
                     "StorageObserverList::MaybeDispatchEvent OnStorageEvent1");

        observer->OnStorageEvent(event);
      } else {
        // When the quota and usage of an origin is requested, QuotaManager
        // returns the quota and usage of the host. Multiple origins can map to
        // to the same host, so ensure the |origin| field in the dispatched
        // event matches the |origin| specified by the observer when it was
        // registered.
        StorageObserver::Event dispatch_event(event);
        dispatch_event.filter.origin = state.origin;

        // crbug.com/349708
        TRACE_EVENT0("io",
                     "StorageObserverList::MaybeDispatchEvent OnStorageEvent2");

        observer->OnStorageEvent(dispatch_event);
      }
    } else {
      all_observers_notified = false;
      base::TimeDelta delay = state.rate - delta;
      if (delay < min_delay)
        min_delay = delay;
    }
  }

  // We need to respect the notification rate specified by observers. So if it
  // is too soon to dispatch an event to an observer, save the event and
  // dispatch it after a delay. If we simply drop the event, another one may
  // not arrive anytime soon and the observer will miss the most recent event.
  if (!all_observers_notified) {
    pending_event_ = event;
    notification_timer_.Start(
        FROM_HERE,
        min_delay,
        this,
        &StorageObserverList::DispatchPendingEvent);
  }
}

void StorageObserverList::ScheduleUpdateForObserver(StorageObserver* observer) {
  DCHECK(base::ContainsKey(observer_state_map_, observer));
  observer_state_map_[observer].requires_update = true;
}

void StorageObserverList::DispatchPendingEvent() {
  MaybeDispatchEvent(pending_event_);
}


// HostStorageObservers:

HostStorageObservers::HostStorageObservers(QuotaManager* quota_manager)
    : quota_manager_(quota_manager),
      initialized_(false),
      initializing_(false),
      event_occurred_before_init_(false),
      usage_deltas_during_init_(0),
      cached_usage_(0),
      cached_quota_(0),
      weak_factory_(this) {
}

HostStorageObservers::~HostStorageObservers() = default;

void HostStorageObservers::AddObserver(
    StorageObserver* observer,
    const StorageObserver::MonitorParams& params) {
  observers_.AddObserver(observer, params);

  if (!params.dispatch_initial_state)
    return;

  if (initialized_) {
    StorageObserver::Event event(params.filter,
                                 std::max<int64_t>(cached_usage_, 0),
                                 std::max<int64_t>(cached_quota_, 0));
    observer->OnStorageEvent(event);
    return;
  }

  // Ensure the observer receives the initial storage state once initialization
  // is complete.
  observers_.ScheduleUpdateForObserver(observer);
  StartInitialization(params.filter);
}

void HostStorageObservers::RemoveObserver(StorageObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool HostStorageObservers::ContainsObservers() const {
  return observers_.ObserverCount() > 0;
}

void HostStorageObservers::NotifyUsageChange(
    const StorageObserver::Filter& filter,
    int64_t delta) {
  if (initialized_) {
    cached_usage_ += delta;
    DispatchEvent(filter, true);
    return;
  }

  // If a storage change occurs before initialization, ensure all observers will
  // receive an event once initialization is complete.
  event_occurred_before_init_ = true;

  // During QuotaManager::GetUsageAndQuotaForWebApps(), cached data is read
  // synchronously, but other data may be retrieved asynchronously. A usage
  // change may occur between the function call and callback. These deltas need
  // to be added to the usage received by GotHostUsageAndQuota() to ensure
  // |cached_usage_| is correctly initialized.
  if (initializing_) {
    usage_deltas_during_init_ += delta;
    return;
  }

  StartInitialization(filter);
}

void HostStorageObservers::StartInitialization(
    const StorageObserver::Filter& filter) {
  if (initialized_ || initializing_)
    return;
  // crbug.com/349708
  TRACE_EVENT0("io", "HostStorageObservers::StartInitialization");

  initializing_ = true;
  quota_manager_->GetUsageAndQuotaForWebApps(
      filter.origin, filter.storage_type,
      base::BindOnce(&HostStorageObservers::GotHostUsageAndQuota,
                     weak_factory_.GetWeakPtr(), filter));
}

void HostStorageObservers::GotHostUsageAndQuota(
    const StorageObserver::Filter& filter,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  initializing_ = false;
  if (status != blink::mojom::QuotaStatusCode::kOk)
    return;
  initialized_ = true;
  cached_quota_ = quota;
  cached_usage_ = usage + usage_deltas_during_init_;
  DispatchEvent(filter, event_occurred_before_init_);
}

void HostStorageObservers::DispatchEvent(
    const StorageObserver::Filter& filter, bool is_update) {
  StorageObserver::Event event(filter, std::max<int64_t>(cached_usage_, 0),
                               std::max<int64_t>(cached_quota_, 0));
  if (is_update)
    observers_.OnStorageChange(event);
  else
    observers_.MaybeDispatchEvent(event);
}


// StorageTypeObservers:

StorageTypeObservers::StorageTypeObservers(QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

StorageTypeObservers::~StorageTypeObservers() = default;

void StorageTypeObservers::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  std::string host = net::GetHostOrSpecFromURL(params.filter.origin.GetURL());
  if (host.empty())
    return;

  auto& host_observers = host_observers_map_[host];
  if (!host_observers) {
    // Because there are no null entries in host_observers_map_, the [] inserted
    // a blank pointer, so let's populate it.
    host_observers = std::make_unique<HostStorageObservers>(quota_manager_);
  }

  host_observers->AddObserver(observer, params);
}

void StorageTypeObservers::RemoveObserver(StorageObserver* observer) {
  for (auto it = host_observers_map_.begin();
       it != host_observers_map_.end();) {
    it->second->RemoveObserver(observer);
    if (!it->second->ContainsObservers()) {
      it = host_observers_map_.erase(it);
    } else {
      ++it;
    }
  }
}

const HostStorageObservers* StorageTypeObservers::GetHostObservers(
    const std::string& host) const {
  auto it = host_observers_map_.find(host);
  if (it != host_observers_map_.end())
    return it->second.get();

  return nullptr;
}

void StorageTypeObservers::NotifyUsageChange(
    const StorageObserver::Filter& filter,
    int64_t delta) {
  std::string host = net::GetHostOrSpecFromURL(filter.origin.GetURL());
  auto it = host_observers_map_.find(host);
  if (it == host_observers_map_.end())
    return;

  it->second->NotifyUsageChange(filter, delta);
}


// StorageMonitor:

StorageMonitor::StorageMonitor(QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

StorageMonitor::~StorageMonitor() = default;

void StorageMonitor::AddObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  DCHECK(observer);

  // Check preconditions.
  if (params.filter.storage_type == blink::mojom::StorageType::kUnknown ||
      params.filter.storage_type ==
          blink::mojom::StorageType::kQuotaNotManaged) {
    NOTREACHED();
    return;
  }

  auto& type_observers =
      storage_type_observers_map_[params.filter.storage_type];
  if (!type_observers)
    type_observers = std::make_unique<StorageTypeObservers>(quota_manager_);

  type_observers->AddObserver(observer, params);
}

void StorageMonitor::RemoveObserver(StorageObserver* observer) {
  for (const auto& type_observers_pair : storage_type_observers_map_)
    type_observers_pair.second->RemoveObserver(observer);
}

const StorageTypeObservers* StorageMonitor::GetStorageTypeObservers(
    blink::mojom::StorageType storage_type) const {
  auto it = storage_type_observers_map_.find(storage_type);
  if (it != storage_type_observers_map_.end())
    return it->second.get();

  return nullptr;
}

void StorageMonitor::NotifyUsageChange(const StorageObserver::Filter& filter,
                                       int64_t delta) {
  // Check preconditions.
  if (filter.storage_type == blink::mojom::StorageType::kUnknown ||
      filter.storage_type == blink::mojom::StorageType::kQuotaNotManaged) {
    NOTREACHED();
    return;
  }

  auto it = storage_type_observers_map_.find(filter.storage_type);
  if (it == storage_type_observers_map_.end())
    return;

  it->second->NotifyUsageChange(filter, delta);
}

}  // namespace storage
