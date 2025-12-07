// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/safety_checks.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

// Gets the DedicatedWorkerServiceImpl, returning nullptr if not possible.
DedicatedWorkerServiceImpl* GetDedicatedWorkerServiceImplForRenderProcessHost(
    RenderProcessHost* worker_process_host) {
  if (!worker_process_host || !worker_process_host->IsInitializedAndNotDead()) {
    // Abort if the worker's process host is gone. This means that the calling
    // frame or worker is also either destroyed or in the process of being
    // destroyed.
    return nullptr;
  }
  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());
  return storage_partition->GetDedicatedWorkerService();
}

}  // namespace

DedicatedWorkerHostFactoryImpl::DedicatedWorkerHostFactoryImpl(
    int worker_process_id,
    DedicatedWorkerCreator creator,
    GlobalRenderFrameHostId ancestor_render_frame_host_id,
    const blink::StorageKey& creator_storage_key,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr creator_client_security_state,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter)
    : worker_process_id_(worker_process_id),
      creator_(creator),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_storage_key_(creator_storage_key),
      isolation_info_(isolation_info),
      creator_client_security_state_(std::move(creator_client_security_state)),
      creator_coep_reporter_(std::move(creator_coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(creator_client_security_state_);
}

DedicatedWorkerHostFactoryImpl::~DedicatedWorkerHostFactoryImpl() = default;

void DedicatedWorkerHostFactoryImpl::CreateWorkerHostAndStartScriptLoad(
    const blink::DedicatedWorkerToken& token,
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient> client,
    net::StorageAccessApiStatus storage_access_api_status) {
  TRACE_EVENT(
      "loading",
      "DedicatedWorkerHostFactoryImpl::CreateWorkerHostAndStartScriptLoad",
      "script_url", script_url);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::TimeTicks start_time = base::TimeTicks::Now();

  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  // Get the dedicated worker service.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  auto* service =
      GetDedicatedWorkerServiceImplForRenderProcessHost(worker_process_host);
  if (!service)
    return;

  if (service->HasToken(token)) {
    mojo::ReportBadMessage("DWH_INVALID_WORKER_TOKEN");
    return;
  }

  // If the renderer claims it has storage access but the browser has no record
  // of granting the permission then deny the request.
  if (storage_access_api_status != net::StorageAccessApiStatus::kNone) {
    RenderFrameHostImpl* ancestor_render_frame_host =
        RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
    if (!ancestor_render_frame_host ||
        !ancestor_render_frame_host->IsFullCookieAccessAllowed()) {
      mojo::ReportBadMessage("DWH_STORAGE_ACCESS_NOT_GRANTED");
      return;
    }
  }

  // TODO(crbug.com/40051700): Compare `creator_storage_key_.origin()` to
  // `script_url`, and report as bad message if that fails.

  mojo::PendingRemote<blink::mojom::DedicatedWorkerHost> pending_remote_host;

  // The origin used by this dedicated worker on the renderer side. This will
  // be the same as the storage key's origin, except in the case of data: URL
  // workers, as described in the linked bug.
  // TODO(crbug.com/40051700): Make the storage key's origin always match this.
  url::Origin renderer_origin = script_url.SchemeIs(url::kDataScheme)
                                    ? url::Origin()
                                    : creator_storage_key_.origin();
  auto* host = new DedicatedWorkerHost(
      service, token, worker_process_host, creator_,
      ancestor_render_frame_host_id_, creator_storage_key_, renderer_origin,
      isolation_info_, std::move(creator_client_security_state_),
      std::move(creator_coep_reporter_),
      pending_remote_host.InitWithNewPipeAndPassReceiver(),
      storage_access_api_status);
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
  host->BindBrowserInterfaceBrokerReceiver(
      broker.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
      std::move(client));
  remote_client->OnWorkerHostCreated(
      std::move(broker), std::move(pending_remote_host), renderer_origin);
  base::UmaHistogramTimes("Worker.BrowserProcess.WorkerHostCreateTime",
                          base::TimeTicks::Now() - start_time);

  base::TimeTicks host_created_time = base::TimeTicks::Now();
  auto devtools_throttle_handle =
      base::MakeRefCounted<DevToolsThrottleHandle>(base::BindOnce(
          &DedicatedWorkerHost::StartScriptLoad, host->GetWeakPtr(), script_url,
          credentials_mode, std::move(outside_fetch_client_settings_object),
          std::move(blob_url_token), std::move(remote_client),
          storage_access_api_status));

  // We are about to start fetching from the browser process and we want
  // devtools to be able to instrument the URLLoaderFactory. This call will
  // create a DevtoolsAgentHost.
  WorkerDevToolsManager::GetInstance().WorkerCreated(
      host, worker_process_host->GetDeprecatedID(),
      ancestor_render_frame_host_id_, std::move(devtools_throttle_handle));
  base::UmaHistogramTimes("Worker.BrowserProcess.StartScriptLoadTime",
                          base::TimeTicks::Now() - start_time);
  base::UmaHistogramTimes("Worker.BrowserProcess.DevToolsCreateTime",
                          base::TimeTicks::Now() - host_created_time);
}

}  // namespace content
