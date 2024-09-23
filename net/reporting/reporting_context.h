// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_CONTEXT_H_
#define NET_REPORTING_REPORTING_CONTEXT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_policy.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace net {

class ReportingCacheObserver;
class ReportingDelegate;
class ReportingDeliveryAgent;
class ReportingGarbageCollector;
class ReportingNetworkChangeObserver;
class ReportingUploader;
class URLRequestContext;

// Contains the various internal classes that make up the Reporting system.
// Wrapped by ReportingService, which provides the external interface.
class NET_EXPORT ReportingContext {
 public:
  // |request_context| and |store| should outlive the ReportingContext.
  static std::unique_ptr<ReportingContext> Create(
      const ReportingPolicy& policy,
      URLRequestContext* request_context,
      ReportingCache::PersistentReportingStore* store,
      const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints);

  ReportingContext(const ReportingContext&) = delete;
  ReportingContext& operator=(const ReportingContext&) = delete;

  virtual ~ReportingContext();

  const ReportingPolicy& policy() const { return policy_; }

  const base::Clock& clock() const { return *clock_; }
  const base::TickClock& tick_clock() const { return *tick_clock_; }
  ReportingUploader* uploader() { return uploader_.get(); }
  ReportingDelegate* delegate() { return delegate_.get(); }
  ReportingCache* cache() { return cache_.get(); }
  ReportingCache::PersistentReportingStore* store() { return store_; }
  ReportingDeliveryAgent* delivery_agent() { return delivery_agent_.get(); }
  ReportingGarbageCollector* garbage_collector() {
    return garbage_collector_.get();
  }

  void AddCacheObserver(ReportingCacheObserver* observer);
  void RemoveCacheObserver(ReportingCacheObserver* observer);

  void NotifyCachedReportsUpdated();
  void NotifyReportAdded(const ReportingReport* report);
  void NotifyReportUpdated(const ReportingReport* report);
  void NotifyCachedClientsUpdated();
  void NotifyEndpointsUpdatedForOrigin(
      const std::vector<ReportingEndpoint>& endpoints);

  // Returns whether the data in the cache is persisted across restarts in the
  // PersistentReportingStore.
  bool IsReportDataPersisted() const;
  bool IsClientDataPersisted() const;

  void OnShutdown();

 protected:
  ReportingContext(
      const ReportingPolicy& policy,
      base::Clock* clock,
      const base::TickClock* tick_clock,
      const RandIntCallback& rand_callback,
      std::unique_ptr<ReportingUploader> uploader,
      std::unique_ptr<ReportingDelegate> delegate,
      ReportingCache::PersistentReportingStore* store,
      const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints);

 private:
  ReportingPolicy policy_;

  raw_ptr<base::Clock> clock_;
  raw_ptr<const base::TickClock> tick_clock_;
  std::unique_ptr<ReportingUploader> uploader_;

  base::ObserverList<ReportingCacheObserver, /* check_empty= */ true>::Unchecked
      cache_observers_;

  std::unique_ptr<ReportingDelegate> delegate_;

  std::unique_ptr<ReportingCache> cache_;

  const raw_ptr<ReportingCache::PersistentReportingStore> store_;

  // |delivery_agent_| must come after |tick_clock_|, |delegate_|, |uploader_|,
  // and |cache_|.
  std::unique_ptr<ReportingDeliveryAgent> delivery_agent_;

  // |garbage_collector_| must come after |tick_clock_| and |cache_|.
  std::unique_ptr<ReportingGarbageCollector> garbage_collector_;

  // |network_change_observer_| must come after |cache_|.
  std::unique_ptr<ReportingNetworkChangeObserver> network_change_observer_;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_CONTEXT_H_
