// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_context.h"

#include <utility>

#include "base/bind.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_delivery_agent.h"
#include "net/reporting/reporting_garbage_collector.h"
#include "net/reporting/reporting_network_change_observer.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_uploader.h"

namespace net {

class URLRequestContext;

namespace {

class ReportingContextImpl : public ReportingContext {
 public:
  ReportingContextImpl(const ReportingPolicy& policy,
                       URLRequestContext* request_context,
                       ReportingCache::PersistentReportingStore* store)
      : ReportingContext(policy,
                         base::DefaultClock::GetInstance(),
                         base::DefaultTickClock::GetInstance(),
                         base::BindRepeating(&base::RandInt),
                         ReportingUploader::Create(request_context),
                         ReportingDelegate::Create(request_context),
                         store) {}
};

}  // namespace

// static
std::unique_ptr<ReportingContext> ReportingContext::Create(
    const ReportingPolicy& policy,
    URLRequestContext* request_context,
    ReportingCache::PersistentReportingStore* store) {
  return std::make_unique<ReportingContextImpl>(policy, request_context, store);
}

ReportingContext::~ReportingContext() = default;

void ReportingContext::AddCacheObserver(ReportingCacheObserver* observer) {
  DCHECK(!cache_observers_.HasObserver(observer));
  cache_observers_.AddObserver(observer);
}

void ReportingContext::RemoveCacheObserver(ReportingCacheObserver* observer) {
  DCHECK(cache_observers_.HasObserver(observer));
  cache_observers_.RemoveObserver(observer);
}

void ReportingContext::NotifyCachedReportsUpdated() {
  for (auto& observer : cache_observers_)
    observer.OnReportsUpdated();
}

void ReportingContext::NotifyCachedClientsUpdated() {
  for (auto& observer : cache_observers_)
    observer.OnClientsUpdated();
}

bool ReportingContext::IsReportDataPersisted() const {
  return store_ && policy_.persist_reports_across_restarts;
}

bool ReportingContext::IsClientDataPersisted() const {
  return store_ && policy_.persist_clients_across_restarts;
}

void ReportingContext::OnShutdown() {
  uploader_->OnShutdown();
}

ReportingContext::ReportingContext(
    const ReportingPolicy& policy,
    base::Clock* clock,
    const base::TickClock* tick_clock,
    const RandIntCallback& rand_callback,
    std::unique_ptr<ReportingUploader> uploader,
    std::unique_ptr<ReportingDelegate> delegate,
    ReportingCache::PersistentReportingStore* store)
    : policy_(policy),
      clock_(clock),
      tick_clock_(tick_clock),
      uploader_(std::move(uploader)),
      delegate_(std::move(delegate)),
      cache_(ReportingCache::Create(this)),
      store_(store),
      delivery_agent_(ReportingDeliveryAgent::Create(this, rand_callback)),
      garbage_collector_(ReportingGarbageCollector::Create(this)),
      network_change_observer_(ReportingNetworkChangeObserver::Create(this)) {}

}  // namespace net
