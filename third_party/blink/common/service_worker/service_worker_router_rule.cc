// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"

#include "base/notreached.h"

namespace blink {

bool ServiceWorkerRouterRequestCondition::operator==(
    const ServiceWorkerRouterRequestCondition& other) const {
  return method == other.method && mode == other.mode &&
         destination == other.destination;
}

bool ServiceWorkerRouterCondition::operator==(
    const ServiceWorkerRouterCondition& other) const {
  if (type != other.type) {
    return false;
  }
  switch (type) {
    case ConditionType::kUrlPattern:
      return url_pattern == other.url_pattern;
    case ConditionType::kRequest:
      return request == other.request;
    case ConditionType::kRunningStatus:
      return running_status == other.running_status;
  }
}

bool ServiceWorkerRouterSource::operator==(
    const ServiceWorkerRouterSource& other) const {
  if (type != other.type) {
    return false;
  }
  switch (type) {
    case SourceType::kNetwork:
      return network_source == other.network_source;
    case SourceType::kRace:
      return race_source == other.race_source;
    case SourceType::kFetchEvent:
      return fetch_event_source == other.fetch_event_source;
  }
}

}  // namespace blink
