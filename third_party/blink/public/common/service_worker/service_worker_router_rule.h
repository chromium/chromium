// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/url_pattern.h"

namespace blink {

// TODO(crbug.com/1371756): implement other conditions in the proposal.
struct ServiceWorkerRouterCondition {
  // Type of conditions.
  enum class ConditionType {
    // URLPattern is used as a condition.
    kUrlPattern,
  };
  ConditionType type;

  // URLPattern to be used for matching.
  // This field is valid if `type` is `kUrlPattern`.
  absl::optional<UrlPattern> url_pattern;
};

// Network source structure.
// TODO(crbug.com/1371756): implement fields in the proposal.
struct ServiceWorkerRouterNetworkSource {};

// This represents a source of the router rule.
// TODO(crbug.com/1371756): implement other sources in the proposal.
struct ServiceWorkerRouterSource {
  // Type of sources.
  enum class SourceType {
    kNetwork,
  };
  SourceType type;

  absl::optional<ServiceWorkerRouterNetworkSource> network_source;
};

// This represents a ServiceWorker static routing API's router rule.
// It represents each route.
struct ServiceWorkerRouterRule {
  // There can be a list of conditions, and expected to be evaluated
  // from front to back.
  std::vector<ServiceWorkerRouterCondition> conditions;
  // There can be a list of sources, and expected to be routed from
  // front to back.
  std::vector<ServiceWorkerRouterSource> sources;
};

// This represents a condition of the router rule.
// This represents a list of ServiceWorker static routing API's router rules.
struct ServiceWorkerRouterRules {
  std::vector<ServiceWorkerRouterRule> rules;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
