// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/common/renderer.mojom.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"

namespace content {

SharedStorageRenderThreadWorkletDriver::SharedStorageRenderThreadWorkletDriver(
    AgentSchedulingGroupHost& agent_scheduling_group_host)
    : agent_scheduling_group_host_(agent_scheduling_group_host) {}

SharedStorageRenderThreadWorkletDriver::
    ~SharedStorageRenderThreadWorkletDriver() = default;

void SharedStorageRenderThreadWorkletDriver::StartWorkletService(
    mojo::PendingReceiver<
        shared_storage_worklet::mojom::SharedStorageWorkletService>
        pending_receiver) {
  agent_scheduling_group_host_.CreateSharedStorageWorkletService(
      std::move(pending_receiver));
}

}  // namespace content
