// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule_mojom_traits.h"
#include "services/network/public/mojom/service_worker_router_info.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom.h"

namespace mojo {

bool StructTraits<
    blink::mojom::ServiceWorkerRouterRunningStatusConditionDataView,
    blink::ServiceWorkerRouterRunningStatusCondition>::
    Read(blink::mojom::ServiceWorkerRouterRunningStatusConditionDataView data,
         blink::ServiceWorkerRouterRunningStatusCondition* out) {
  if (!data.ReadStatus(&out->status)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterOrConditionDataView,
                  blink::ServiceWorkerRouterOrCondition>::
    Read(blink::mojom::ServiceWorkerRouterOrConditionDataView data,
         blink::ServiceWorkerRouterOrCondition* out) {
  if (!data.ReadConditions(&out->conditions)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterNotConditionDataView,
                  blink::ServiceWorkerRouterNotCondition>::
    Read(blink::mojom::ServiceWorkerRouterNotConditionDataView data,
         blink::ServiceWorkerRouterNotCondition* out) {
  blink::ServiceWorkerRouterCondition condition;
  if (!data.ReadCondition(&condition)) {
    return false;
  }
  out->condition = std::make_unique<blink::ServiceWorkerRouterCondition>(
      std::move(condition));
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterRequestConditionDataView,
                  blink::ServiceWorkerRouterRequestCondition>::
    Read(blink::mojom::ServiceWorkerRouterRequestConditionDataView data,
         blink::ServiceWorkerRouterRequestCondition* out) {
  if (!data.ReadMethod(&out->method)) {
    return false;
  }
  if (data.has_mode()) {
    out->mode = data.mode();
  }
  if (data.has_destination()) {
    out->destination = data.destination();
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterConditionDataView,
                  blink::ServiceWorkerRouterCondition>::
    Read(blink::mojom::ServiceWorkerRouterConditionDataView data,
         blink::ServiceWorkerRouterCondition* out) {
  auto&& [url_pattern, request, running_status, or_condition, not_condition] =
      out->get();
  if (!data.ReadUrlPattern(&url_pattern)) {
    return false;
  }
  if (!data.ReadRequest(&request)) {
    return false;
  }
  if (!data.ReadRunningStatus(&running_status)) {
    return false;
  }
  if (!data.ReadOrCondition(&or_condition)) {
    return false;
  }
  if (!data.ReadNotCondition(&not_condition)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterRaceSourceDataView,
                  blink::ServiceWorkerRouterRaceSource>::
    Read(blink::mojom::ServiceWorkerRouterRaceSourceDataView data,
         blink::ServiceWorkerRouterRaceSource* out) {
  if (!data.ReadTarget(&out->target)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterCacheSourceDataView,
                  blink::ServiceWorkerRouterCacheSource>::
    Read(blink::mojom::ServiceWorkerRouterCacheSourceDataView data,
         blink::ServiceWorkerRouterCacheSource* out) {
  if (!data.ReadCacheName(&out->cache_name)) {
    return false;
  }
  return true;
}

blink::mojom::ServiceWorkerRouterSourceDataView::Tag
UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
            blink::ServiceWorkerRouterSource>::
    GetTag(const blink::ServiceWorkerRouterSource& data) {
  switch (data.type) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return blink::mojom::ServiceWorkerRouterSource::Tag::kNetworkSource;
    case network::mojom::ServiceWorkerRouterSourceType::kRace:
      return blink::mojom::ServiceWorkerRouterSource::Tag::kRaceSource;
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return blink::mojom::ServiceWorkerRouterSource::Tag::kFetchEventSource;
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      return blink::mojom::ServiceWorkerRouterSource::Tag::kCacheSource;
  }
}

bool UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
                 blink::ServiceWorkerRouterSource>::
    Read(blink::mojom::ServiceWorkerRouterSourceDataView data,
         blink::ServiceWorkerRouterSource* out) {
  switch (data.tag()) {
    case blink::mojom::ServiceWorkerRouterSource::Tag::kNetworkSource:
      out->type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      out->network_source.emplace();
      return true;
    case blink::mojom::ServiceWorkerRouterSource::Tag::kRaceSource:
      out->type = network::mojom::ServiceWorkerRouterSourceType::kRace;
      out->race_source.emplace();
      return true;
    case blink::mojom::ServiceWorkerRouterSource::Tag::kFetchEventSource:
      out->type = network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
      out->fetch_event_source.emplace();
      return true;
    case blink::mojom::ServiceWorkerRouterSource::Tag::kCacheSource:
      out->type = network::mojom::ServiceWorkerRouterSourceType::kCache;
      if (!data.ReadCacheSource(&out->cache_source)) {
        return false;
      }
      return true;
  }
  return false;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterRuleDataView,
                  blink::ServiceWorkerRouterRule>::
    Read(blink::mojom::ServiceWorkerRouterRuleDataView data,
         blink::ServiceWorkerRouterRule* out) {
  if (!data.ReadCondition(&out->condition)) {
    return false;
  }
  if (!data.ReadSources(&out->sources)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterRulesDataView,
                  blink::ServiceWorkerRouterRules>::
    Read(blink::mojom::ServiceWorkerRouterRulesDataView data,
         blink::ServiceWorkerRouterRules* out) {
  if (!data.ReadRules(&out->rules)) {
    return false;
  }
  return true;
}

}  // namespace mojo
