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

network::mojom::blink::ServiceWorkerRouterInfoPtr
ServiceWorkerRouterInfo::ToMojo() const {
  network::mojom::blink::ServiceWorkerRouterInfoPtr info =
      network::mojom::blink::ServiceWorkerRouterInfo::New();
  info->rule_id_matched = rule_id_matched_;
  return info;
}

}  // namespace blink
