// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_
#define SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "net/base/request_priority.h"
#include "services/network/resource_scheduler/resource_scheduler.h"

namespace net {
class URLRequest;
class NetworkQualityEstimator;
}  // namespace net

namespace network {

// ResourceSchedulerClient represents ResourceScheduler::Client. At this
// moment it uses two integers (child_id and route_id) to specify a client
// for compatibility with content/browser/loader users, but this should be
// changed to a less error-prone interface once Network Service is launched.
// A ResourceSchedulerClient instance can be shared, but it must not outlive
// the associated ResourceScheduler. Failing to keep the contract will be
// detected by a DCHECK in ResourceScheduler's destructor which checks if all
// clients are removed.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResourceSchedulerClient final
    : public base::RefCounted<ResourceSchedulerClient> {
 public:
  ResourceSchedulerClient(int child_id,
                          int route_id,
                          ResourceScheduler* resource_scheduler,
                          net::NetworkQualityEstimator* estimator);

  std::unique_ptr<ResourceScheduler::ScheduledResourceRequest> ScheduleRequest(
      bool is_async,
      net::URLRequest* url_request);
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

 private:
  friend class base::RefCounted<ResourceSchedulerClient>;
  ~ResourceSchedulerClient();

  const int child_id_;
  const int route_id_;
  ResourceScheduler* const resource_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ResourceSchedulerClient);
};
}  // namespace network

#endif  // SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_CLIENT_H_
