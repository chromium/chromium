// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/gpu_host/gpu_host_test_api.h"

#include <algorithm>

#include "components/viz/host/gpu_client.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "services/ws/gpu_host/gpu_host.h"

namespace ws {
namespace gpu_host {

GpuHostTestApi::GpuHostTestApi(GpuHost* gpu_host) : gpu_host_(gpu_host) {}

GpuHostTestApi::~GpuHostTestApi() = default;

void GpuHostTestApi::SetGpuService(viz::mojom::GpuServicePtr gpu_service) {
  return viz::GpuHostImplTestApi(gpu_host_->gpu_host_impl_.get())
      .SetGpuService(std::move(gpu_service));
}

base::WeakPtr<viz::GpuClient> GpuHostTestApi::GetLastGpuClient() {
  if (gpu_host_->gpu_clients_.empty())
    return nullptr;
  return gpu_host_->gpu_clients_.back()->GetWeakPtr();
}

}  // namespace gpu_host
}  // namespace ws
