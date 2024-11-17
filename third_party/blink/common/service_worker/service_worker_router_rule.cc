// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"

namespace blink {

bool ServiceWorkerRouterRequestCondition::operator==(
    const ServiceWorkerRouterRequestCondition& other) const {
  return method == other.method && mode == other.mode &&
         destination == other.destination;
}

bool ServiceWorkerRouterOrCondition::operator==(
    const ServiceWorkerRouterOrCondition& other) const {
  return conditions == other.conditions;
}

ServiceWorkerRouterNotCondition::ServiceWorkerRouterNotCondition() = default;
ServiceWorkerRouterNotCondition::~ServiceWorkerRouterNotCondition() = default;
ServiceWorkerRouterNotCondition::ServiceWorkerRouterNotCondition(
    const ServiceWorkerRouterNotCondition& other) {
  *this = other;
}
ServiceWorkerRouterNotCondition::ServiceWorkerRouterNotCondition(
    ServiceWorkerRouterNotCondition&&) = default;

ServiceWorkerRouterNotCondition& ServiceWorkerRouterNotCondition::operator=(
    const ServiceWorkerRouterNotCondition& other) {
  if (other.condition) {
    condition =
        std::make_unique<ServiceWorkerRouterCondition>(*other.condition);
  }
  return *this;
}
ServiceWorkerRouterNotCondition& ServiceWorkerRouterNotCondition::operator=(
    ServiceWorkerRouterNotCondition&&) = default;

bool ServiceWorkerRouterNotCondition::operator==(
    const ServiceWorkerRouterNotCondition& other) const {
  // Returns false unless both have their value.
  return condition && other.condition && *condition == *other.condition;
}

bool ServiceWorkerRouterCondition::operator==(
    const ServiceWorkerRouterCondition& other) const {
  return get() == other.get();
}

bool ServiceWorkerRouterRaceSource::operator==(
    const ServiceWorkerRouterRaceSource& other) const {
  return target == other.target;
}

bool ServiceWorkerRouterCacheSource::operator==(
    const ServiceWorkerRouterCacheSource& other) const {
  return cache_name == other.cache_name;
}

bool ServiceWorkerRouterSource::operator==(
    const ServiceWorkerRouterSource& other) const {
  if (type != other.type) {
    return false;
  }
  switch (type) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return network_source == other.network_source;
    case network::mojom::ServiceWorkerRouterSourceType::kRace:
      return race_source == other.race_source;
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return fetch_event_source == other.fetch_event_source;
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      return cache_source == other.cache_source;
  }
}

}  // namespace blink
