// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom.h"
#include "third_party/blink/public/mojom/url_pattern.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::ServiceWorkerRouterConditionDataView,
                blink::ServiceWorkerRouterCondition> {
  static blink::mojom::ServiceWorkerRouterConditionDataView::Tag GetTag(
      const blink::ServiceWorkerRouterCondition& data);

  static const blink::UrlPattern& url_pattern(
      const blink::ServiceWorkerRouterCondition& data) {
    return *data.url_pattern;
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
    UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
                blink::ServiceWorkerRouterSource> {
  static blink::mojom::ServiceWorkerRouterSourceDataView::Tag GetTag(
      const blink::ServiceWorkerRouterSource& data);

  static const blink::ServiceWorkerRouterNetworkSource& network_source(
      const blink::ServiceWorkerRouterSource& data) {
    return *data.network_source;
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
