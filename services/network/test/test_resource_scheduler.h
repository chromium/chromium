// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_RESOURCE_SCHEDULER_H_
#define SERVICES_NETWORK_TEST_TEST_RESOURCE_SCHEDULER_H_

#include "services/network/resource_scheduler/resource_scheduler.h"

namespace network {

// Noop implementation of mojom::NetworkContext.  Useful to override to create
// specialized mocks or fakes.
class TestResourceScheduler : public ResourceScheduler {
 public:
  explicit TestResourceScheduler(const base::TickClock* tick_clock = nullptr) {}
  ~TestResourceScheduler() override = default;

  TestResourceScheduler(const TestResourceScheduler&) = delete;
  TestResourceScheduler& operator=(const TestResourceScheduler&) = delete;

  std::unique_ptr<ScheduledResourceRequest> ScheduleRequest(
      int child_id,
      int route_id,
      bool is_async,
      net::URLRequest* url_request) override {
    return nullptr;
  }

  void OnClientCreated(
      int child_id,
      int route_id,
      net::NetworkQualityEstimator* network_quality_estimator) override {}

  void OnClientDeleted(int child_id, int route_id) override {}

  size_t ActiveSchedulerClientsCounter() const override { return 0; }

  void RecordGlobalRequestCountMetrics() const override {}

  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value) override {}

  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority) override {}

  bool IsLongQueuedRequestsDispatchTimerRunning() const override {
    return false;
  }

  base::SequencedTaskRunner* task_runner() override { return nullptr; }
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_RESOURCE_SCHEDULER_H_
