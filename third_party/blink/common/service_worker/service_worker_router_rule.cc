// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"

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

bool ServiceWorkerRouterCondition::operator==(
    const ServiceWorkerRouterCondition& other) const {
  if (type != other.type) {
    return false;
  }
  switch (type) {
    case Type::kUrlPattern:
      return url_pattern == other.url_pattern;
    case Type::kRequest:
      return request == other.request;
    case Type::kRunningStatus:
      return running_status == other.running_status;
    case Type::kOr:
      return or_condition == other.or_condition;
  }
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
    case Type::kNetwork:
      return network_source == other.network_source;
    case Type::kRace:
      return race_source == other.race_source;
    case Type::kFetchEvent:
      return fetch_event_source == other.fetch_event_source;
    case Type::kCache:
      return cache_source == other.cache_source;
  }
}

}  // namespace blink
