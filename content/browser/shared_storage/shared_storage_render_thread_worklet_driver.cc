// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "url/origin.h"

namespace content {

SharedStorageRenderThreadWorkletDriver::SharedStorageRenderThreadWorkletDriver(
    RenderFrameHost& render_frame_host,
    const url::Origin& data_origin) {
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_frame_host.GetStoragePartition());

  // Leave the worklet SiteInstance as NonIsolated(), as the cross-origin
  // isolated capability is currently unspecified in the spec:
  // https://html.spec.whatwg.org/multipage/worklets.html#script-settings-for-worklets%3Aconcept-settings-object-cross-origin-isolated-capability
  //
  // TODO(yaoxia): This may need to be revisited in the future.
  UrlInfo url_info(
      UrlInfoInit(data_origin.GetURL())
          .WithStoragePartitionConfig(storage_partition->GetConfig())
          .WithWebExposedIsolationInfo(
              WebExposedIsolationInfo::CreateNonIsolated()));

  // We aim to approximate the process allocation behavior of iframes when
  // loading a URL with origin `data_origin`.
  //
  // Leverage the existing process creation and tracking approach for service
  // workers, with an additional call to `ReuseExistingProcessIfPossible()` that
  // allows the worklet to reuse the initiator frame's process (as in Android's
  // relaxed site isolation).
  //
  // TODO(yaoxia): Refactor into a
  // `SiteInstanceImpl::CreateForSharedStorageWorklet` method. That will require
  // renaming several downstream components, such as
  // `UnmatchedServiceWorkerProcessTracker`.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForServiceWorker(
          storage_partition->browser_context(), url_info,
          /*can_reuse_process=*/true, storage_partition->is_guest(),
          render_frame_host.IsNestedWithinFencedFrame());

  site_instance->ReuseExistingProcessIfPossible(render_frame_host.GetProcess());

  // TODO(yaoxia): Gracefully handle Init() error?
  site_instance->GetProcess()->Init();

  site_instance->GetProcess()->AddObserver(this);

  if (!site_instance->GetProcess()->AreRefCountsDisabled()) {
    site_instance->GetProcess()->IncrementWorkerRefCount();
  }

  site_instance_ = site_instance;
}

SharedStorageRenderThreadWorkletDriver::
    ~SharedStorageRenderThreadWorkletDriver() {
  // The render process is already destroyed. No further action is needed.
  if (!site_instance_) {
    return;
  }

  GetProcessHost()->RemoveObserver(this);

  if (!GetProcessHost()->AreRefCountsDisabled()) {
    GetProcessHost()->DecrementWorkerRefCount();
  }
}

void SharedStorageRenderThreadWorkletDriver::StartWorkletService(
    mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService>
        pending_receiver,
    blink::mojom::WorkletGlobalScopeCreationParamsPtr
        global_scope_creation_params) {
  // `StartWorkletService` will be called right after the driver is created when
  // the document is still alive, as the driver is created on-demand on the
  // first worklet operation. Thus, `site_instance_` should always be valid at
  // this point.
  DCHECK(site_instance_);

  static_cast<SiteInstanceImpl&>(*site_instance_)
      .GetOrCreateAgentSchedulingGroup()
      .CreateSharedStorageWorkletService(
          std::move(pending_receiver), std::move(global_scope_creation_params));
}

RenderProcessHost* SharedStorageRenderThreadWorkletDriver::GetProcessHost() {
  if (!site_instance_) {
    return nullptr;
  }

  return site_instance_->GetProcess();
}

void SharedStorageRenderThreadWorkletDriver::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  // This could occur when the browser shuts down during the worklet's
  // keep-alive phase, or when the renderer process is terminated. Reset
  // `site_instance_` to signal this state change. Note that calling
  // GetProcessHost() again here would not return the
  // original process host, so we wouldn't be able to assert `host` here.
  host->RemoveObserver(this);
  site_instance_ = nullptr;
}

}  // namespace content
