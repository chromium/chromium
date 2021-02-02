// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"

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
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    base::Optional<blink::DedicatedWorkerToken> creator_worker_token,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const net::IsolationInfo& isolation_info,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter)
    : worker_process_id_(worker_process_id),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      creator_worker_token_(creator_worker_token),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_origin),
      isolation_info_(isolation_info),
      cross_origin_embedder_policy_(cross_origin_embedder_policy),
      coep_reporter_(std::move(coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK((creator_render_frame_host_id_ && !creator_worker_token_) ||
         (!creator_render_frame_host_id_ && creator_worker_token_));
}

DedicatedWorkerHostFactoryImpl::~DedicatedWorkerHostFactoryImpl() = default;

void DedicatedWorkerHostFactoryImpl::CreateWorkerHost(
    const blink::DedicatedWorkerToken& token,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> broker_receiver,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver,
    base::OnceCallback<void(const network::CrossOriginEmbedderPolicy&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Always invoke the callback first. If we don't, even if we exit with a
  // mojo::ReportBadMessage, the callback will explode as it is torn down.
  // Ideally we'd have a handle to our binding and we'd manually close it
  // before returning, letting the callback die without being run.
  std::move(callback).Run(cross_origin_embedder_policy_);

  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
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

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  auto* host = new DedicatedWorkerHost(
      service, token, worker_process_host, creator_render_frame_host_id_,
      creator_worker_token_, ancestor_render_frame_host_id_, creator_origin_,
      isolation_info_, cross_origin_embedder_policy_, std::move(coep_reporter),
      std::move(host_receiver));
  host->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
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

  // TODO(https://crbug.com/1058759): Compare |creator_origin_| to
  // |script_url|, and report as bad message if that fails.

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::DedicatedWorkerHost> pending_remote_host;
  auto* host = new DedicatedWorkerHost(
      service, token, worker_process_host, creator_render_frame_host_id_,
      creator_worker_token_, ancestor_render_frame_host_id_, creator_origin_,
      isolation_info_, cross_origin_embedder_policy_, std::move(coep_reporter),
      pending_remote_host.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
  host->BindBrowserInterfaceBrokerReceiver(
      broker.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
      std::move(client));
  remote_client->OnWorkerHostCreated(std::move(broker),
                                     std::move(pending_remote_host));
  host->StartScriptLoad(script_url, credentials_mode,
                        std::move(outside_fetch_client_settings_object),
                        std::move(blob_url_token), std::move(remote_client));
}

}  // namespace content
