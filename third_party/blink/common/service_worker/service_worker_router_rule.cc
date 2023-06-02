// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"

#include "base/notreached.h"

namespace blink {

bool ServiceWorkerRouterCondition::operator==(
    const ServiceWorkerRouterCondition& other) const {
  switch (type) {
    case ConditionType::kUrlPattern:
      return url_pattern == other.url_pattern;
  }
}

bool ServiceWorkerRouterSource::operator==(
    const ServiceWorkerRouterSource& other) const {
  switch (type) {
    case SourceType::kNetwork:
      return network_source == other.network_source;
  }
}

}  // namespace blink
