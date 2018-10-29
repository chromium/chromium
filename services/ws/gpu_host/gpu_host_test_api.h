// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_GPU_HOST_GPU_HOST_TEST_API_H_
#define SERVICES_WS_GPU_HOST_GPU_HOST_TEST_API_H_

#include "base/memory/weak_ptr.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"

namespace viz {
class GpuClient;
}

namespace ws {
namespace gpu_host {
class GpuHost;

class GpuHostTestApi {
 public:
  GpuHostTestApi(GpuHost* gpu_host);
  ~GpuHostTestApi();

  void SetGpuService(viz::mojom::GpuServicePtr gpu_service);
  base::WeakPtr<viz::GpuClient> GetLastGpuClient();

 private:
  GpuHost* gpu_host_;

  DISALLOW_COPY_AND_ASSIGN(GpuHostTestApi);
};

}  // namespace gpu_host
}  // namespace ws

#endif  // SERVICES_WS_GPU_HOST_GPU_HOST_TEST_API_H_
