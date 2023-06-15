// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule_mojom_traits.h"

namespace mojo {

blink::mojom::ServiceWorkerRouterConditionDataView::Tag
UnionTraits<blink::mojom::ServiceWorkerRouterConditionDataView,
            blink::ServiceWorkerRouterCondition>::
    GetTag(const blink::ServiceWorkerRouterCondition& data) {
  switch (data.type) {
    case blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern:
      return blink::mojom::ServiceWorkerRouterCondition::Tag::kUrlPattern;
  }
}

bool UnionTraits<blink::mojom::ServiceWorkerRouterConditionDataView,
                 blink::ServiceWorkerRouterCondition>::
    Read(blink::mojom::ServiceWorkerRouterConditionDataView data,
         blink::ServiceWorkerRouterCondition* out) {
  switch (data.tag()) {
    case blink::mojom::ServiceWorkerRouterCondition::Tag::kUrlPattern:
      if (!data.ReadUrlPattern(&out->url_pattern)) {
        return false;
      }
      return true;
  }

  return false;
}

blink::mojom::ServiceWorkerRouterSourceDataView::Tag
UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
            blink::ServiceWorkerRouterSource>::
    GetTag(const blink::ServiceWorkerRouterSource& data) {
  switch (data.type) {
    case blink::ServiceWorkerRouterSource::SourceType::kNetwork:
      return blink::mojom::ServiceWorkerRouterSource::Tag::kNetworkSource;
  }
}

bool UnionTraits<blink::mojom::ServiceWorkerRouterSourceDataView,
                 blink::ServiceWorkerRouterSource>::
    Read(blink::mojom::ServiceWorkerRouterSourceDataView data,
         blink::ServiceWorkerRouterSource* out) {
  switch (data.tag()) {
    case blink::mojom::ServiceWorkerRouterSource::Tag::kNetworkSource:
      out->network_source.emplace();
      return true;
  }
  return false;
}

bool StructTraits<blink::mojom::ServiceWorkerRouterRuleDataView,
                  blink::ServiceWorkerRouterRule>::
    Read(blink::mojom::ServiceWorkerRouterRuleDataView data,
         blink::ServiceWorkerRouterRule* out) {
  if (!data.ReadConditions(&out->conditions)) {
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
