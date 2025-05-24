// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-blink.h"

namespace blink {

ServiceWorkerRouterInfo::ServiceWorkerRouterInfo() = default;

scoped_refptr<ServiceWorkerRouterInfo> ServiceWorkerRouterInfo::Create() {
  return base::AdoptRef(new ServiceWorkerRouterInfo);
}

String ServiceWorkerRouterInfo::GetRouterSourceTypeString(
    const network::mojom::ServiceWorkerRouterSourceType source) {
  switch (source) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return "network";
    case network::mojom::ServiceWorkerRouterSourceType::
        kRaceNetworkAndFetchEvent:
      return "race-network-and-fetch-handler";
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      return "cache";
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return "fetch-event";
    case network::mojom::ServiceWorkerRouterSourceType::kRaceNetworkAndCache:
      return "race-network-and-cache";
  }
}

network::mojom::blink::ServiceWorkerRouterInfoPtr
ServiceWorkerRouterInfo::ToMojo() const {
  network::mojom::blink::ServiceWorkerRouterInfoPtr info =
      network::mojom::blink::ServiceWorkerRouterInfo::New();
  info->rule_id_matched = rule_id_matched_;
  info->matched_source_type = matched_source_type_;
  info->actual_source_type = actual_source_type_;
  info->route_rule_num = route_rule_num_;
  info->evaluation_worker_status = evaluation_worker_status_;
  info->router_evaluation_time = router_evaluation_time_;
  return info;
}

}  // namespace blink
