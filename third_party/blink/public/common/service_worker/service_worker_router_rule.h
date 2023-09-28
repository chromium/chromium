// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/safe_url_pattern.h"

namespace network::mojom {

enum class RequestMode : int32_t;
enum class RequestDestination : int32_t;

}  // namespace network::mojom

namespace blink {

struct ServiceWorkerRouterCondition;

struct ServiceWorkerRouterRequestCondition {
  // https://fetch.spec.whatwg.org/#concept-request-method
  // Technically, it can be an arbitrary string, but Chromium would set
  // k*Method in net/http/http_request_headers.h
  absl::optional<std::string> method;
  // RequestMode in services/network/public/mojom/fetch_api.mojom
  absl::optional<network::mojom::RequestMode> mode;
  // RequestDestination in services/network/public/mojom/fetch_api.mojom
  absl::optional<network::mojom::RequestDestination> destination;

  bool operator==(const ServiceWorkerRouterRequestCondition& other) const;
};

struct ServiceWorkerRouterRunningStatusCondition {
  enum class RunningStatusEnum {
    kRunning = 0,
    // This includes kStarting and kStopping in addition to kStopped.
    // These states are consolidated to kNotRunning because they need to
    // wait for ServiceWorker set up to run the fetch handler.
    kNotRunning = 1,
  };

  RunningStatusEnum status;

  bool operator==(
      const ServiceWorkerRouterRunningStatusCondition& other) const {
    return status == other.status;
  }
};

struct ServiceWorkerRouterOrCondition {
  std::vector<ServiceWorkerRouterCondition> conditions;

  bool operator==(const ServiceWorkerRouterOrCondition& other) const;
};

// TODO(crbug.com/1371756): implement other conditions in the proposal.
// TODO(crbug.com/1456599): migrate to absl::variant if possible.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterCondition {
  // Type of conditions.
  enum class Type {
    // URLPattern is used as a condition.
    kUrlPattern,
    // Request condition.
    kRequest,
    // Running status condition.
    kRunningStatus,
    // Or condition
    kOr,
  };
  Type type;

  // URLPattern to be used for matching.
  // This field is valid if `type` is `kUrlPattern`.
  absl::optional<SafeUrlPattern> url_pattern;

  // Request to be used for matching.
  // This field is valid if `type` is `kRequest`.
  absl::optional<ServiceWorkerRouterRequestCondition> request;

  // Running status to be used for matching.
  // This field is valid if `type` is `kRunningStatus`.
  absl::optional<ServiceWorkerRouterRunningStatusCondition> running_status;

  // `Or` condition to be used for matching
  // This field is valid if `type` is `kOr`
  // We need `_condition` suffix to avoid conflict with reserved keywords in C++
  absl::optional<ServiceWorkerRouterOrCondition> or_condition;

  bool operator==(const ServiceWorkerRouterCondition& other) const;
};

// Network source structure.
// TODO(crbug.com/1371756): implement fields in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterNetworkSource {
  bool operator==(const ServiceWorkerRouterNetworkSource& other) const {
    return true;
  }
};

// Race network and fetch handler source.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRaceSource {
  bool operator==(const ServiceWorkerRouterRaceSource& other) const {
    return true;
  }
};

// Fetch handler source structure.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterFetchEventSource {
  bool operator==(const ServiceWorkerRouterFetchEventSource& other) const {
    return true;
  }
};

// Cache source structure.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterCacheSource {
  // A name of the Cache object.
  // If the field is not set, any of the Cache objects that the CacheStorage
  // tracks are used for matching as if CacheStorage.match().
  absl::optional<std::string> cache_name;

  bool operator==(const ServiceWorkerRouterCacheSource& other) const;
};

// This represents a source of the router rule.
// TODO(crbug.com/1371756): implement other sources in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterSource {
  // Type of sources.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Type {
    // Network is used as a source.
    kNetwork = 0,
    // Race network and fetch handler.
    kRace = 1,
    // Fetch Event is used as a source.
    kFetchEvent = 2,
    // Cache is used as a source.
    kCache = 3,

    kMaxValue = kCache,
  };
  Type type;

  absl::optional<ServiceWorkerRouterNetworkSource> network_source;
  absl::optional<ServiceWorkerRouterRaceSource> race_source;
  absl::optional<ServiceWorkerRouterFetchEventSource> fetch_event_source;
  absl::optional<ServiceWorkerRouterCacheSource> cache_source;

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
