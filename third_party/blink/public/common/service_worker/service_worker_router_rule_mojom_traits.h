// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"

#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT EnumTraits<
    blink::mojom::ServiceWorkerRouterRunningStatusEnum,
    blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum> {
  static blink::mojom::ServiceWorkerRouterRunningStatusEnum ToMojom(
      blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum
          input) {
    switch (input) {
      case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
          kRunning:
        return blink::mojom::ServiceWorkerRouterRunningStatusEnum::kRunning;
      case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
          kNotRunning:
        return blink::mojom::ServiceWorkerRouterRunningStatusEnum::kNotRunning;
    }
  }
  static bool FromMojom(
      blink::mojom::ServiceWorkerRouterRunningStatusEnum input,
      blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum*
          output) {
    switch (input) {
      case blink::mojom::ServiceWorkerRouterRunningStatusEnum::kRunning:
        *output = blink::ServiceWorkerRouterRunningStatusCondition::
            RunningStatusEnum::kRunning;
        break;
      case blink::mojom::ServiceWorkerRouterRunningStatusEnum::kNotRunning:
        *output = blink::ServiceWorkerRouterRunningStatusCondition::
            RunningStatusEnum::kNotRunning;
        break;
    }
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::ServiceWorkerRouterRunningStatusConditionDataView,
    blink::ServiceWorkerRouterRunningStatusCondition> {
  static blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum
  status(const blink::ServiceWorkerRouterRunningStatusCondition& data) {
    return data.status;
  }

  static bool Read(
      blink::mojom::ServiceWorkerRouterRunningStatusConditionDataView data,
      blink::ServiceWorkerRouterRunningStatusCondition* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterRequestConditionDataView,
                 blink::ServiceWorkerRouterRequestCondition> {
  static const absl::optional<std::string>& method(
      const blink::ServiceWorkerRouterRequestCondition& data) {
    return data.method;
  }

  static bool has_mode(const blink::ServiceWorkerRouterRequestCondition& data) {
    return data.mode.has_value();
  }

  static network::mojom::RequestMode mode(
      const blink::ServiceWorkerRouterRequestCondition& data) {
    if (!data.mode) {
      // This value should not be used but returning the default value.
      return network::mojom::RequestMode::kNoCors;
    }
    return *data.mode;
  }

  static bool has_destination(
      const blink::ServiceWorkerRouterRequestCondition& data) {
    return data.destination.has_value();
  }

  static network::mojom::RequestDestination destination(
      const blink::ServiceWorkerRouterRequestCondition& data) {
    if (!data.destination) {
      // This value should not be used but returning the default value.
      return network::mojom::RequestDestination::kEmpty;
    }
    return *data.destination;
  }

  static bool Read(
      blink::mojom::ServiceWorkerRouterRequestConditionDataView data,
      blink::ServiceWorkerRouterRequestCondition* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterOrConditionDataView,
                 blink::ServiceWorkerRouterOrCondition> {
  static const std::vector<blink::ServiceWorkerRouterCondition>& conditions(
      const blink::ServiceWorkerRouterOrCondition& data) {
    return data.conditions;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterOrConditionDataView data,
                   blink::ServiceWorkerRouterOrCondition* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::ServiceWorkerRouterConditionDataView,
                blink::ServiceWorkerRouterCondition> {
  static blink::mojom::ServiceWorkerRouterConditionDataView::Tag GetTag(
      const blink::ServiceWorkerRouterCondition& data);

  static const blink::SafeUrlPattern& url_pattern(
      const blink::ServiceWorkerRouterCondition& data) {
    return *data.url_pattern;
  }

  static const blink::ServiceWorkerRouterRequestCondition& request(
      const blink::ServiceWorkerRouterCondition& data) {
    return *data.request;
  }

  static const blink::ServiceWorkerRouterRunningStatusCondition& running_status(
      const blink::ServiceWorkerRouterCondition& data) {
    return *data.running_status;
  }

  static const blink::ServiceWorkerRouterOrCondition& or_condition(
      const blink::ServiceWorkerRouterCondition& data) {
    return *data.or_condition;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterConditionDataView data,
                   blink::ServiceWorkerRouterCondition* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterNetworkSourceDataView,
                 blink::ServiceWorkerRouterNetworkSource> {
  static bool Read(blink::mojom::ServiceWorkerRouterNetworkSourceDataView data,
                   blink::ServiceWorkerRouterNetworkSource* out) {
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterRaceSourceDataView,
                 blink::ServiceWorkerRouterRaceSource> {
  static bool Read(blink::mojom::ServiceWorkerRouterRaceSourceDataView data,
                   blink::ServiceWorkerRouterRaceSource* out) {
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterFetchEventSourceDataView,
                 blink::ServiceWorkerRouterFetchEventSource> {
  static bool Read(
      blink::mojom::ServiceWorkerRouterFetchEventSourceDataView data,
      blink::ServiceWorkerRouterFetchEventSource* out) {
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterCacheSourceDataView,
                 blink::ServiceWorkerRouterCacheSource> {
  static const absl::optional<std::string>& cache_name(
      const blink::ServiceWorkerRouterCacheSource& data) {
    return data.cache_name;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterCacheSourceDataView data,
                   blink::ServiceWorkerRouterCacheSource* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
                blink::ServiceWorkerRouterSource> {
  static blink::mojom::ServiceWorkerRouterSourceDataView::Tag GetTag(
      const blink::ServiceWorkerRouterSource& data);

  static const blink::ServiceWorkerRouterNetworkSource& network_source(
      const blink::ServiceWorkerRouterSource& data) {
    return *data.network_source;
  }

  static const blink::ServiceWorkerRouterRaceSource& race_source(
      const blink::ServiceWorkerRouterSource& data) {
    return *data.race_source;
  }

  static const blink::ServiceWorkerRouterFetchEventSource& fetch_event_source(
      const blink::ServiceWorkerRouterSource& data) {
    return *data.fetch_event_source;
  }

  static const blink::ServiceWorkerRouterCacheSource& cache_source(
      const blink::ServiceWorkerRouterSource& data) {
    return *data.cache_source;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterSourceDataView data,
                   blink::ServiceWorkerRouterSource* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterRuleDataView,
                 blink::ServiceWorkerRouterRule> {
  static const std::vector<blink::ServiceWorkerRouterCondition>& conditions(
      const blink::ServiceWorkerRouterRule& in) {
    return in.conditions;
  }

  static const std::vector<blink::ServiceWorkerRouterSource>& sources(
      const blink::ServiceWorkerRouterRule& in) {
    return in.sources;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterRuleDataView data,
                   blink::ServiceWorkerRouterRule* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ServiceWorkerRouterRulesDataView,
                 blink::ServiceWorkerRouterRules> {
  static const std::vector<blink::ServiceWorkerRouterRule>& rules(
      const blink::ServiceWorkerRouterRules& in) {
    return in.rules;
  }

  static bool Read(blink::mojom::ServiceWorkerRouterRulesDataView data,
                   blink::ServiceWorkerRouterRules* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_MOJOM_TRAITS_H_
