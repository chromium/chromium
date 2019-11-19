// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/network_isolation_key.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {

DedicatedWorkerHost::DedicatedWorkerHost(
    int worker_process_id,
    int ancestor_render_frame_id,
    int creator_render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host)
    : worker_process_id_(worker_process_id),
      ancestor_render_frame_id_(ancestor_render_frame_id),
      creator_render_frame_id_(creator_render_frame_id),
      origin_(origin),
      host_receiver_(this, std::move(host)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RegisterMojoInterfaces();
}

DedicatedWorkerHost::~DedicatedWorkerHost() = default;

void DedicatedWorkerHost::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host)
    return;

  // See if the registry that is specific to this worker host wants to handle
  // the interface request.
  if (registry_.TryBindInterface(interface_name, &interface_pipe))
    return;

  BindWorkerInterface(interface_name, std::move(interface_pipe),
                      worker_process_host, origin_);
}

void DedicatedWorkerHost::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
}

void DedicatedWorkerHost::LifecycleStateChanged(
    blink::mojom::FrameLifecycleState state) {
  switch (state) {
    case blink::mojom::FrameLifecycleState::kFrozen:
    case blink::mojom::FrameLifecycleState::kFrozenAutoResumeMedia:
      is_frozen_ = true;
      break;
    case blink::mojom::FrameLifecycleState::kRunning:
      is_frozen_ = false;
      break;
    case blink::mojom::FrameLifecycleState::kPaused:
      // This shouldn't be reached, the render process does not send this
      // state.
      NOTREACHED();
      break;
  }
}

void DedicatedWorkerHost::StartScriptLoad(
    const GURL& script_url,
    const url::Origin& request_initiator_origin,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  DCHECK(!client_);
  DCHECK(client);
  client_ = std::move(client);

  // Get a storage partition.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());

  // Get nearest ancestor render frame host in order to determine the
  // top-frame origin to use for the network isolation key.
  RenderFrameHostImpl* nearest_ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!nearest_ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // Walk up the RenderFrameHostImpl::GetParent() chain to get to the top
  // RenderFrameHostImpl, instead of using the frame tree node.
  // If the root has already navigated to a different render frame host by
  // the time that we get here, the old root render frame host should still
  // be around in pending deletion state (i.e. running its unload handler)
  // and reachable via this walk even though it's no longer the same as
  // root()->current_frame_host(). The old root render frame host will still
  // have its old origin in GetLastCommittedOrigin(). See crbug.com/986167
  RenderFrameHostImpl* top_frame = nullptr;
  for (RenderFrameHostImpl* frame = nearest_ancestor_render_frame_host; frame;
       frame = frame->GetParent()) {
    top_frame = frame;
  }

  // Compute the network isolation key using the old root's last committed
  // origin as top-frame origin.
  url::Origin top_frame_origin(top_frame->GetLastCommittedOrigin());
  url::Origin current_frame_origin(
      nearest_ancestor_render_frame_host->GetLastCommittedOrigin());
  network_isolation_key_ =
      net::NetworkIsolationKey(top_frame_origin, current_frame_origin);

  // Get a storage domain.
  SiteInstance* site_instance =
      nearest_ancestor_render_frame_host->GetSiteInstance();
  if (!site_instance) {
    client_->OnScriptLoadStartFailed();
    return;
  }
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_impl->browser_context(), site_instance->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (script_url.SchemeIsBlob()) {
    if (!blob_url_token) {
      mojo::ReportBadMessage("DWH_NO_BLOB_URL_TOKEN");
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            storage_partition_impl->browser_context(),
            std::move(blob_url_token));
  } else if (blob_url_token) {
    mojo::ReportBadMessage("DWH_NOT_BLOB_URL");
    return;
  }

  // If this is a nested worker, there is no creator frame.
  RenderFrameHostImpl* creator_render_frame_host = nullptr;
  if (creator_render_frame_id_ != MSG_ROUTING_NONE) {
    // Use |worker_process_id_| as the creator render frame's process ID as the
    // frame surely lives in the same process for dedicated workers.
    creator_render_frame_host = RenderFrameHostImpl::FromID(
        worker_process_id_, creator_render_frame_id_);
    if (!creator_render_frame_host) {
      client_->OnScriptLoadStartFailed();
      return;
    }
  }

  // A dedicated worker depends on its nearest ancestor's appcache host.
  AppCacheHost* appcache_host = nullptr;
  const AppCacheNavigationHandle* appcache_handle =
      nearest_ancestor_render_frame_host->GetAppCacheNavigationHandle();
  if (appcache_handle) {
    appcache_host = storage_partition_impl->GetAppCacheService()->GetHost(
        appcache_handle->appcache_host_id());
  }

  // Set if the subresource loader factories support file URLs so that we can
  // recreate the factories after Network Service crashes.
  // TODO(nhiroki): Currently this flag is calculated based on the request
  // initiator origin to keep consistency with WorkerScriptFetchInitiator, but
  // probably this should be calculated based on the worker origin as the
  // factories be used for subresource loading on the worker.
  file_url_support_ = request_initiator_origin.scheme() == url::kFileScheme;

  service_worker_handle_ = std::make_unique<ServiceWorkerNavigationHandle>(
      storage_partition_impl->GetServiceWorkerContext());

  WorkerScriptFetchInitiator::Start(
      worker_process_id_, script_url, creator_render_frame_host,
      request_initiator_origin, network_isolation_key_, credentials_mode,
      std::move(outside_fetch_client_settings_object), ResourceType::kWorker,
      storage_partition_impl->GetServiceWorkerContext(),
      service_worker_handle_.get(),
      appcache_host ? appcache_host->GetWeakPtr() : nullptr,
      std::move(blob_url_loader_factory), nullptr, storage_partition_impl,
      storage_domain,
      base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::RegisterMojoInterfaces() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registry_.AddInterface(base::BindRepeating(
      &DedicatedWorkerHost::CreateWebSocketConnector, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &DedicatedWorkerHost::CreateWebUsbService, base::Unretained(this)));
  registry_.AddInterface(
      base::BindRepeating(&DedicatedWorkerHost::CreateNestedDedicatedWorker,
                          base::Unretained(this)));
}

void DedicatedWorkerHost::DidStartScriptLoad(
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  if (!success) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // TODO(https://crbug.com/986188): Check if the main script's final response
  // URL is commitable.

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // TODO(cammie): Change this approach when we support shared workers
  // creating dedicated workers, as there might be no ancestor frame.
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // Start observing Network Service crash when it's running out-of-process.
  if (IsOutOfProcessNetworkService()) {
    ObserveNetworkServiceCrash(static_cast<StoragePartitionImpl*>(
        worker_process_host->GetStoragePartition()));
  }

  // Set up the default network loader factory.
  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(worker_process_host,
                                          ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  // Prepare the controller service worker info to pass to the renderer.
  // |object_info| can be nullptr when the service worker context or the
  // service worker version is gone during dedicated worker startup.
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
      service_worker_remote_object;
  blink::mojom::ServiceWorkerState service_worker_state;
  if (controller && controller->object_info) {
    controller->object_info->receiver =
        service_worker_remote_object.InitWithNewEndpointAndPassReceiver();
    service_worker_state = controller->object_info->state;
  }

  client_->OnScriptLoadStarted(
      service_worker_handle_->TakeProviderInfo(),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories),
      subresource_loader_updater_.BindNewPipeAndPassReceiver(),
      std::move(controller));

  // |service_worker_remote_object| is an associated remote, so calls can't be
  // made on it until its receiver is sent. Now that the receiver was sent, it
  // can be used, so add it to ServiceWorkerObjectHost.
  if (service_worker_remote_object) {
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
            controller_service_worker_object_host,
            std::move(service_worker_remote_object), service_worker_state));
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DedicatedWorkerHost::CreateNetworkFactoryForSubresources(
    RenderProcessHost* worker_process_host,
    RenderFrameHostImpl* ancestor_render_frame_host,
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(worker_process_host);
  DCHECK(ancestor_render_frame_host);
  DCHECK(bypass_redirect_checks);

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      default_header_client;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      storage_partition_impl->browser_context(),
      /*frame=*/nullptr, worker_process_id_,
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource, origin_,
      &default_factory_receiver, &default_header_client,
      bypass_redirect_checks);

  // TODO(nhiroki): Call devtools_instrumentation::WillCreateURLLoaderFactory()
  // here.

  worker_process_host->CreateURLLoaderFactory(
      origin_, origin_,
      ancestor_render_frame_host->cross_origin_embedder_policy(),
      /*preferences=*/nullptr, network_isolation_key_,
      std::move(default_header_client), std::move(default_factory_receiver));

  return pending_default_factory;
}

void DedicatedWorkerHost::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  // The ancestor frame may have already been closed. In that case, the worker
  // will soon be terminated too, so abort the connection.
  if (!ancestor_render_frame_host)
    return;

  ancestor_render_frame_host->CreateWebUsbService(std::move(receiver));
}

void DedicatedWorkerHost::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    receiver.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                             "The parent frame has already been gone.");
    return;
  }
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  worker_process_id_, ancestor_render_frame_id_,
                                  origin_, network_isolation_key_),
                              std::move(receiver));
}

void DedicatedWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(worker_process_id_, origin_,
                                                   network_isolation_key_),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateNestedDedicatedWorker(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateDedicatedWorkerHostFactory(worker_process_id_,
                                   ancestor_render_frame_id_,
                                   /*creator_render_frame_id=*/MSG_ROUTING_NONE,
                                   origin_, std::move(receiver));
}

void DedicatedWorkerHost::CreateIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }
  if (!ancestor_render_frame_host->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
    mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
    return;
  }
  static_cast<StoragePartitionImpl*>(
      ancestor_render_frame_host->GetProcess()->GetStoragePartition())
      ->GetIdleManager()
      ->CreateService(std::move(receiver));
}

void DedicatedWorkerHost::CreateIDBFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* worker_process_host = GetProcessHost();
  if (!worker_process_host)
    return;
  worker_process_host->BindIndexedDB(MSG_ROUTING_NONE, GetOrigin(),
                                     std::move(receiver));
}

void DedicatedWorkerHost::BindSmsReceiverReceiver(
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindSmsReceiverReceiver(std::move(receiver));
}

#if !defined(OS_ANDROID)
void DedicatedWorkerHost::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      GetAncestorRenderFrameHost();
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindSerialService(std::move(receiver));
}
#endif

void DedicatedWorkerHost::ObserveNetworkServiceCrash(
    StoragePartitionImpl* storage_partition_impl) {
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = worker_process_id_;
  network_service_connection_error_handler_holder_.reset();
  storage_partition_impl->GetNetworkContext()->CreateURLLoaderFactory(
      network_service_connection_error_handler_holder_
          .BindNewPipeAndPassReceiver(),
      std::move(params));
  network_service_connection_error_handler_holder_.set_disconnect_handler(
      base::BindOnce(&DedicatedWorkerHost::UpdateSubresourceLoaderFactories,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::UpdateSubresourceLoaderFactories() {
  DCHECK(IsOutOfProcessNetworkService());
  DCHECK(subresource_loader_updater_.is_bound());
  DCHECK(network_service_connection_error_handler_holder_);
  DCHECK(!network_service_connection_error_handler_holder_.is_connected());

  // Get a storage partition.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host)
    return;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());

  // Get a storage domain.
  if (!GetAncestorRenderFrameHost() ||
      !GetAncestorRenderFrameHost()->GetSiteInstance())
    return;
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_impl->browser_context(),
      GetAncestorRenderFrameHost()->GetSiteInstance()->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  // Start observing Network Service crash again.
  ObserveNetworkServiceCrash(storage_partition_impl);

  // Recreate the default URLLoaderFactory. This doesn't support
  // AppCache-specific factory.
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories =
          WorkerScriptFetchInitiator::CreateFactoryBundle(
              WorkerScriptFetchInitiator::LoaderType::kSubResource,
              worker_process_id_, storage_partition_impl, storage_domain,
              file_url_support_, /*filesystem_url_support=*/true);

  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(worker_process_host,
                                          GetAncestorRenderFrameHost(),
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  subresource_loader_updater_->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

// May return a nullptr.
RenderFrameHostImpl* DedicatedWorkerHost::GetAncestorRenderFrameHost() {
  // Use |worker_process_id_| as the ancestor render frame's process ID as the
  // frame surely lives in the same process for dedicated workers.
  const int ancestor_render_frame_process_id = worker_process_id_;
  return RenderFrameHostImpl::FromID(ancestor_render_frame_process_id,
                                     ancestor_render_frame_id_);
}

namespace {
// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by the
// renderer over mojo via SelfOwnedReceiver. It lives on the UI thread.
class DedicatedWorkerHostFactoryImpl final
    : public blink::mojom::DedicatedWorkerHostFactory {
 public:
  DedicatedWorkerHostFactoryImpl(int creator_process_id,
                                 int ancestor_render_frame_id,
                                 int creator_render_frame_id,
                                 const url::Origin& parent_context_origin)
      : creator_process_id_(creator_process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        creator_render_frame_id_(creator_render_frame_id),
        parent_context_origin_(parent_context_origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // blink::mojom::DedicatedWorkerHostFactory:
  void CreateWorkerHost(
      const url::Origin& origin,
      service_manager::mojom::InterfaceProviderRequest request,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
      mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
      return;
    }

    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    auto host = std::make_unique<DedicatedWorkerHost>(
        creator_process_id_, ancestor_render_frame_id_,
        creator_render_frame_id_, origin, std::move(host_receiver));
    host->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
    mojo::MakeSelfOwnedReceiver(
        std::move(host), FilterRendererExposedInterfaces(
                             blink::mojom::kNavigation_DedicatedWorkerSpec,
                             creator_process_id_, std::move(request)));
  }

  // PlzDedicatedWorker:
  void CreateWorkerHostAndStartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient>
          client,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
      mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
      return;
    }

    // Create a worker host that will start a new dedicated worker in the
    // renderer process whose ID is |creator_process_id_|.
    //
    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    auto host = std::make_unique<DedicatedWorkerHost>(
        creator_process_id_, ancestor_render_frame_id_,
        creator_render_frame_id_, request_initiator_origin,
        std::move(host_receiver));
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
    host->BindBrowserInterfaceBrokerReceiver(
        broker.InitWithNewPipeAndPassReceiver());
    auto* host_raw = host.get();
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    mojo::MakeSelfOwnedReceiver(
        std::move(host),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, creator_process_id_,
            mojo::MakeRequest(&interface_provider)));

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
        std::move(client));
    remote_client->OnWorkerHostCreated(std::move(interface_provider),
                                       std::move(broker));
    host_raw->StartScriptLoad(
        script_url, request_initiator_origin, credentials_mode,
        std::move(outside_fetch_client_settings_object),
        std::move(blob_url_token), std::move(remote_client));
  }

 private:
  // See comments on the corresponding members of DedicatedWorkerHost.
  const int creator_process_id_;
  const int ancestor_render_frame_id_;
  const int creator_render_frame_id_;

  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHostFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int creator_process_id,
    int ancestor_render_frame_id,
    int creator_render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(std::make_unique<DedicatedWorkerHostFactoryImpl>(
                                  creator_process_id, ancestor_render_frame_id,
                                  creator_render_frame_id, origin),
                              std::move(receiver));
}

}  // namespace content
