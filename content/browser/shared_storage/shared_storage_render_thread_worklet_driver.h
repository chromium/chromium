// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_

#include "content/browser/shared_storage/shared_storage_worklet_driver.h"

namespace content {

class AgentSchedulingGroupHost;

// The worklet driver that starts the worklet service on a dedicated render
// thread on the same render process that the worklet's owner document belongs
// to.
//
// The lifetime of the WorkletDriver is tied to the RenderFrameHost associated
// with the worklet's owner document.
class SharedStorageRenderThreadWorkletDriver
    : public SharedStorageWorkletDriver {
 public:
  explicit SharedStorageRenderThreadWorkletDriver(
      AgentSchedulingGroupHost& agent_scheduling_group_host);
  ~SharedStorageRenderThreadWorkletDriver() override;

  // SharedStorageWorkletDriver overrides
  void StartWorkletService(
      mojo::PendingReceiver<
          shared_storage_worklet::mojom::SharedStorageWorkletService>
          pending_receiver) override;

 private:
  // Responsible for initializing the worklet service.
  AgentSchedulingGroupHost& agent_scheduling_group_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
