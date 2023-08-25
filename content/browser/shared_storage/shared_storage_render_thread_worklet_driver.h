// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"

#include "content/public/browser/render_process_host_observer.h"

namespace content {

class AgentSchedulingGroupHost;
class RenderProcessHost;

// The worklet driver that starts the worklet service on a dedicated render
// thread on the same render process that the worklet's owner document belongs
// to.
//
// The lifetime of the WorkletDriver is tied to the `SharedStorageWorkletHost`.
class SharedStorageRenderThreadWorkletDriver
    : public SharedStorageWorkletDriver,
      public RenderProcessHostObserver {
 public:
  explicit SharedStorageRenderThreadWorkletDriver(
      AgentSchedulingGroupHost* agent_scheduling_group_host);
  ~SharedStorageRenderThreadWorkletDriver() override;

  // SharedStorageWorkletDriver overrides
  void StartWorkletService(
      mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService>
          pending_receiver,
      blink::mojom::WorkletGlobalScopeCreationParamsPtr
          global_scope_creation_params) override;
  RenderProcessHost* GetProcessHost() override;

  // RenderProcessHostObserver overrides
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 private:
  // Responsible for initializing the worklet service and for incrementing/
  // decrementing the `RenderProcessHost`'s keep-alive counter. This will be
  // initialized to a valid `AgentSchedulingGroupHost` when the driver is
  // created, and will be reset to nullptr when the `AgentSchedulingGroupHost`
  // is about to be destroyed.
  //
  // TODO(crbug.com/1141459): Once AgentSchedulingGroupHostObserver exists, we
  // need to use it to track the lifetime of `agent_scheduling_group_host_`
  // instead of RenderProcessHostObserver. For more context see
  // crbug.com/1141459#c4.
  raw_ptr<AgentSchedulingGroupHost> agent_scheduling_group_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
