// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_policy.h"

#include "base/no_destructor.h"
#include "base/time/time.h"

namespace net {

namespace {

ReportingPolicy* policy_for_testing = nullptr;

}  // namespace

// static
std::unique_ptr<ReportingPolicy> ReportingPolicy::Create() {
  if (policy_for_testing != nullptr) {
    return std::make_unique<ReportingPolicy>(*policy_for_testing);
  }
  return std::make_unique<ReportingPolicy>();
}

// static
void ReportingPolicy::UsePolicyForTesting(const ReportingPolicy& policy) {
  static base::NoDestructor<ReportingPolicy> owned_policy;
  policy_for_testing = owned_policy.get();
  *owned_policy = policy;
}

ReportingPolicy::ReportingPolicy()
    : max_report_count(100u),
      max_endpoint_count(1000u),
      max_endpoints_per_origin(40u),
      delivery_interval(base::TimeDelta::FromMinutes(1)),
      persistence_interval(base::TimeDelta::FromMinutes(1)),
      persist_reports_across_restarts(false),
      persist_clients_across_restarts(true),
      garbage_collection_interval(base::TimeDelta::FromMinutes(5)),
      max_report_age(base::TimeDelta::FromMinutes(15)),
      max_group_staleness(base::TimeDelta::FromDays(7)),
      max_report_attempts(5),
      persist_reports_across_network_changes(false),
      persist_clients_across_network_changes(true) {
  endpoint_backoff_policy.num_errors_to_ignore = 0;
  endpoint_backoff_policy.initial_delay_ms = 60 * 1000;  // 1 minute
  endpoint_backoff_policy.multiply_factor = 2.0;
  endpoint_backoff_policy.jitter_factor = 0.1;
  endpoint_backoff_policy.maximum_backoff_ms = -1;  // 1 hour
  endpoint_backoff_policy.entry_lifetime_ms = -1;   // infinite
  endpoint_backoff_policy.always_use_initial_delay = false;
}

ReportingPolicy::ReportingPolicy(const ReportingPolicy& other) = default;

ReportingPolicy::~ReportingPolicy() = default;

}  // namespace net
