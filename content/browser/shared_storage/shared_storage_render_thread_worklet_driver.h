// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/public/browser/render_process_host_observer.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;
class RenderProcessHost;
class SiteInstance;

// The worklet driver that starts the worklet service. It first picks or
// allocates a renderer process based on the calling context and the
// data_origin, and then creates a thread on the chosen renderer process. The
// thread will be used to host the worklet service.
//
// The lifetime of the WorkletDriver is tied to the `SharedStorageWorkletHost`.
//
// TODO(crbug.com/40154232): Once AgentSchedulingGroupHostObserver exists, we
// need to use it to track the lifetime of `agent_scheduling_group_host_`
// instead of RenderProcessHostObserver. For more context see
// crbug.com/1141459#c4.
class SharedStorageRenderThreadWorkletDriver
    : public SharedStorageWorkletDriver,
      public RenderProcessHostObserver {
 public:
  explicit SharedStorageRenderThreadWorkletDriver(
      RenderFrameHost& render_frame_host,
      const url::Origin& data_origin);
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
  // The site instance for this worklet. This will be initialized when the
  // driver is created, and will be reset when the `RenderProcessHost` is about
  // to be destroyed.
  scoped_refptr<SiteInstance> site_instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RENDER_THREAD_WORKLET_DRIVER_H_
