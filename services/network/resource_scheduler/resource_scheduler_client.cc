// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler/resource_scheduler_client.h"

#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"

namespace network {

ResourceSchedulerClient::ResourceSchedulerClient(
    int child_id,
    int route_id,
    ResourceScheduler* resource_scheduler,
    net::NetworkQualityEstimator* estimator)
    : child_id_(child_id),
      route_id_(route_id),
      resource_scheduler_(resource_scheduler) {
  resource_scheduler_->OnClientCreated(child_id_, route_id_, estimator);
}

ResourceSchedulerClient::~ResourceSchedulerClient() {
  resource_scheduler_->OnClientDeleted(child_id_, route_id_);
}

std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
ResourceSchedulerClient::ScheduleRequest(bool is_async,
                                         net::URLRequest* url_request) {
  return resource_scheduler_->ScheduleRequest(child_id_, route_id_, is_async,
                                              url_request);
}

void ResourceSchedulerClient::ReprioritizeRequest(
    net::URLRequest* request,
    net::RequestPriority new_priority,
    int intra_priority_value) {
  resource_scheduler_->ReprioritizeRequest(request, new_priority,
                                           intra_priority_value);
}

}  // namespace network
