// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/features.h"
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
    absl::optional<GlobalRenderFrameHostId> creator_render_frame_host_id,
    absl::optional<blink::DedicatedWorkerToken> creator_worker_token,
    GlobalRenderFrameHostId ancestor_render_frame_host_id,
    const blink::StorageKey& creator_storage_key,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr creator_client_security_state,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> ancestor_coep_reporter)
    : worker_process_id_(worker_process_id),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      creator_worker_token_(creator_worker_token),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_storage_key_(creator_storage_key),
      isolation_info_(isolation_info),
      creator_client_security_state_(std::move(creator_client_security_state)),
      creator_coep_reporter_(std::move(creator_coep_reporter)),
      ancestor_coep_reporter_(std::move(ancestor_coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(creator_render_frame_host_id_.has_value(),
            creator_worker_token_.has_value());
  DCHECK(creator_client_security_state_);
}

DedicatedWorkerHostFactoryImpl::~DedicatedWorkerHostFactoryImpl() = default;

void DedicatedWorkerHostFactoryImpl::CreateWorkerHost(
    const blink::DedicatedWorkerToken& token,
    const GURL& script_url,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> broker_receiver,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver,
    CreateWorkerHostCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Always invoke the callback. If we don't, even if we exit with a
  // mojo::ReportBadMessage, the callback will explode as it is torn down.
  // Ideally we'd have a handle to our binding and we'd manually close it
  // before returning, letting the callback die without being run.
  DCHECK(callback);

  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    std::move(callback).Run(
        creator_client_security_state_->cross_origin_embedder_policy,
        /*back_forward_cache_controller_host=*/mojo::NullRemote());
    mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
    return;
  }

  // Get the dedicated worker service.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  auto* service =
      GetDedicatedWorkerServiceImplForRenderProcessHost(worker_process_host);
  if (!service) {
    std::move(callback).Run(
        creator_client_security_state_->cross_origin_embedder_policy,
        /*back_forward_cache_controller_host=*/mojo::NullRemote());
    return;
  }

  if (service->HasToken(token)) {
    std::move(callback).Run(
        creator_client_security_state_->cross_origin_embedder_policy,
        /*back_forward_cache_controller_host=*/mojo::NullRemote());
    mojo::ReportBadMessage("DWH_INVALID_WORKER_TOKEN");
    return;
  }

  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy =
      creator_client_security_state_->cross_origin_embedder_policy;

  auto* host = new DedicatedWorkerHost(
      service, token, worker_process_host, creator_render_frame_host_id_,
      creator_worker_token_, ancestor_render_frame_host_id_,
      creator_storage_key_, isolation_info_,
      std::move(creator_client_security_state_),
      std::move(creator_coep_reporter_), std::move(ancestor_coep_reporter_),
      std::move(host_receiver));
  host->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
  host->MaybeCountWebFeature(script_url);

  std::move(callback).Run(
      cross_origin_embedder_policy,
      host->BindAndPassRemoteForBackForwardCacheControllerHost());
}

// PlzDedicatedWorker:
void DedicatedWorkerHostFactoryImpl::CreateWorkerHostAndStartScriptLoad(
    const blink::DedicatedWorkerToken& token,
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient>
        client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
    return;
  }

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

  // TODO(https://crbug.com/1058759): Compare `creator_storage_key_.origin()` to
  // `script_url`, and report as bad message if that fails.

  mojo::PendingRemote<blink::mojom::DedicatedWorkerHost> pending_remote_host;
  auto* host = new DedicatedWorkerHost(
      service, token, worker_process_host, creator_render_frame_host_id_,
      creator_worker_token_, ancestor_render_frame_host_id_,
      creator_storage_key_, isolation_info_,
      std::move(creator_client_security_state_),
      std::move(creator_coep_reporter_), std::move(ancestor_coep_reporter_),
      pending_remote_host.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
  host->BindBrowserInterfaceBrokerReceiver(
      broker.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
      std::move(client));
  remote_client->OnWorkerHostCreated(std::move(broker),
                                     std::move(pending_remote_host));

  auto devtools_throttle_handle =
      base::MakeRefCounted<DevToolsThrottleHandle>(base::BindOnce(
          &DedicatedWorkerHost::StartScriptLoad, host->GetWeakPtr(), script_url,
          credentials_mode, std::move(outside_fetch_client_settings_object),
          std::move(blob_url_token), std::move(remote_client)));

  // We are about to start fetching from the browser process and we want
  // devtools to be able to instrument the URLLoaderFactory. This call will
  // create a DevtoolsAgentHost.
  WorkerDevToolsManager::GetInstance().WorkerCreated(
      host, worker_process_host->GetID(), ancestor_render_frame_host_id_,
      std::move(devtools_throttle_handle));
}

}  // namespace content
