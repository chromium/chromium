// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_STORAGE_MONITOR_H_
#define STORAGE_BROWSER_QUOTA_STORAGE_MONITOR_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "storage/browser/quota/storage_observer.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {
class StorageMonitorTestBase;
}

namespace storage {

class QuotaManager;

// This class dispatches storage events to observers of a common
// StorageObserver::Filter.
class STORAGE_EXPORT StorageObserverList {
 public:
  StorageObserverList();
  virtual ~StorageObserverList();

  // Adds/removes an observer.
  void AddObserver(StorageObserver* observer,
                   const StorageObserver::MonitorParams& params);
  void RemoveObserver(StorageObserver* observer);

  // Returns the number of observers.
  int ObserverCount() const;

  // Forwards a storage change to observers. The event may be dispatched
  // immediately to an observer or after a delay, depending on the desired event
  // rate of the observer.
  void OnStorageChange(const StorageObserver::Event& event);

  // Dispatch an event to observers that require it.
  void MaybeDispatchEvent(const StorageObserver::Event& event);

  // Ensure the specified observer receives the next dispatched event.
  void ScheduleUpdateForObserver(StorageObserver* observer);

 private:
  struct STORAGE_EXPORT ObserverState {
    url::Origin origin;
    base::TimeTicks last_notification_time;
    base::TimeDelta rate;
    bool requires_update;

    ObserverState();
  };

  void DispatchPendingEvent();

  std::map<StorageObserver*, ObserverState> observer_state_map_;
  base::OneShotTimer notification_timer_;
  StorageObserver::Event pending_event_;

  friend class content::StorageMonitorTestBase;

  DISALLOW_COPY_AND_ASSIGN(StorageObserverList);
};


// Manages the storage observers of a common host. Caches the usage and quota of
// the host to avoid accumulating for every change.
class STORAGE_EXPORT HostStorageObservers {
 public:
  explicit HostStorageObservers(QuotaManager* quota_manager);
  virtual ~HostStorageObservers();

  bool is_initialized() const { return initialized_; }

  // Adds/removes an observer.
  void AddObserver(
      StorageObserver* observer,
      const StorageObserver::MonitorParams& params);
  void RemoveObserver(StorageObserver* observer);
  bool ContainsObservers() const;

  // Handles a usage change.
  void NotifyUsageChange(const StorageObserver::Filter& filter, int64_t delta);

 private:
  void StartInitialization(const StorageObserver::Filter& filter);
  void GotHostUsageAndQuota(const StorageObserver::Filter& filter,
                            blink::mojom::QuotaStatusCode status,
                            int64_t usage,
                            int64_t quota);
  void DispatchEvent(const StorageObserver::Filter& filter, bool is_update);

  QuotaManager* quota_manager_;
  StorageObserverList observers_;

  // Flags used during initialization of the cached properties.
  bool initialized_;
  bool initializing_;
  bool event_occurred_before_init_;
  int64_t usage_deltas_during_init_;

  // Cached accumulated usage and quota for the host.
  int64_t cached_usage_;
  int64_t cached_quota_;

  base::WeakPtrFactory<HostStorageObservers> weak_factory_;

  friend class content::StorageMonitorTestBase;

  DISALLOW_COPY_AND_ASSIGN(HostStorageObservers);
};


// Manages the observers of a common storage type.
class STORAGE_EXPORT StorageTypeObservers {
 public:
  explicit StorageTypeObservers(QuotaManager* quota_manager);
  virtual ~StorageTypeObservers();

  // Adds and removes an observer.
  void AddObserver(StorageObserver* observer,
                   const StorageObserver::MonitorParams& params);
  void RemoveObserver(StorageObserver* observer);

  // Returns the observers of a specific host.
  const HostStorageObservers* GetHostObservers(const std::string& host) const;

  // Handles a usage change.
  void NotifyUsageChange(const StorageObserver::Filter& filter, int64_t delta);

 private:
  QuotaManager* quota_manager_;
  std::map<std::string, std::unique_ptr<HostStorageObservers>>
      host_observers_map_;

  DISALLOW_COPY_AND_ASSIGN(StorageTypeObservers);
};


// Storage monitor manages observers and dispatches storage events to them.
class STORAGE_EXPORT StorageMonitor {
 public:
  explicit StorageMonitor(QuotaManager* quota_manager);
  virtual ~StorageMonitor();

  // Adds and removes an observer.
  void AddObserver(StorageObserver* observer,
                   const StorageObserver::MonitorParams& params);
  void RemoveObserver(StorageObserver* observer);
  void RemoveObserverForFilter(StorageObserver* observer,
                               const StorageObserver::Filter& filter);

  // Returns the observers of a specific storage type.
  const StorageTypeObservers* GetStorageTypeObservers(
      blink::mojom::StorageType storage_type) const;

  // Handles a usage change.
  void NotifyUsageChange(const StorageObserver::Filter& filter, int64_t delta);

 private:
  QuotaManager* quota_manager_;
  std::map<blink::mojom::StorageType, std::unique_ptr<StorageTypeObservers>>
      storage_type_observers_map_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitor);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_STORAGE_MONITOR_H_
