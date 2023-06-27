// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/url_pattern.h"

namespace blink {

// TODO(crbug.com/1371756): implement other conditions in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterCondition {
  // Type of conditions.
  enum class ConditionType {
    // URLPattern is used as a condition.
    kUrlPattern,
  };
  ConditionType type;

  // URLPattern to be used for matching.
  // This field is valid if `type` is `kUrlPattern`.
  absl::optional<UrlPattern> url_pattern;

  bool operator==(const ServiceWorkerRouterCondition& other) const;
};

// Network source structure.
// TODO(crbug.com/1371756): implement fields in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterNetworkSource {
  bool operator==(const ServiceWorkerRouterNetworkSource& other) const {
    return true;
  }
};

// This represents a source of the router rule.
// TODO(crbug.com/1371756): implement other sources in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterSource {
  // Type of sources.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SourceType {
    // Network is used as a source.
    kNetwork = 0,

    kMaxValue = kNetwork,
  };
  SourceType type;

  absl::optional<ServiceWorkerRouterNetworkSource> network_source;

  bool operator==(const ServiceWorkerRouterSource& other) const;
};

// This represents a ServiceWorker static routing API's router rule.
// It represents each route.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRule {
  // There can be a list of conditions, and expected to be evaluated
  // from front to back.
  std::vector<ServiceWorkerRouterCondition> conditions;
  // There can be a list of sources, and expected to be routed from
  // front to back.
  std::vector<ServiceWorkerRouterSource> sources;

  bool operator==(const ServiceWorkerRouterRule& other) const {
    return conditions == other.conditions && sources == other.sources;
  }
};

// This represents a condition of the router rule.
// This represents a list of ServiceWorker static routing API's router rules.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRules {
  std::vector<ServiceWorkerRouterRule> rules;

  bool operator==(const ServiceWorkerRouterRules& other) const {
    return rules == other.rules;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
