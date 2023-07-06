// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_
#define SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "net/base/request_priority.h"
#include "services/network/is_browser_initiated.h"
#include "services/network/resource_scheduler/resource_scheduler.h"

namespace net {
class URLRequest;
class NetworkQualityEstimator;
}  // namespace net

namespace network {

// ResourceSchedulerClient represents ResourceScheduler::Client. It uses
// ResourceScheduler::ClientId to specify a client.
// A ResourceSchedulerClient instance can be shared, but it must not outlive
// the associated ResourceScheduler. Failing to keep the contract will be
// detected by a DCHECK in ResourceScheduler's destructor which checks if all
// clients are removed.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResourceSchedulerClient final
    : public base::RefCounted<ResourceSchedulerClient> {
 public:
  ResourceSchedulerClient(ResourceScheduler::ClientId id,
                          IsBrowserInitiated is_browser_initiated,
                          ResourceScheduler* resource_scheduler,
                          net::NetworkQualityEstimator* estimator);

  ResourceSchedulerClient(const ResourceSchedulerClient&) = delete;
  ResourceSchedulerClient& operator=(const ResourceSchedulerClient&) = delete;

  std::unique_ptr<ResourceScheduler::ScheduledResourceRequest> ScheduleRequest(
      bool is_async,
      net::URLRequest* url_request);
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

 private:
  friend class base::RefCounted<ResourceSchedulerClient>;
  ~ResourceSchedulerClient();

  const ResourceScheduler::ClientId id_;
  const raw_ptr<ResourceScheduler> resource_scheduler_;
};
}  // namespace network

#endif  // SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_
