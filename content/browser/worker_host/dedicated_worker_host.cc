// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/content_security_notifier.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"

namespace content {

DedicatedWorkerHost::DedicatedWorkerHost(
    DedicatedWorkerServiceImpl* service,
    const blink::DedicatedWorkerToken& token,
    RenderProcessHost* worker_process_host,
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    base::Optional<blink::DedicatedWorkerToken> creator_worker_token,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const net::IsolationInfo& isolation_info,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host)
    : service_(service),
      token_(token),
      worker_process_host_(worker_process_host),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      creator_worker_token_(creator_worker_token),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_origin),
      // TODO(https://crbug.com/1058759): Calculate the worker origin based on
      // the worker script URL.
      worker_origin_(creator_origin),
      isolation_info_(isolation_info),
      creator_cross_origin_embedder_policy_(cross_origin_embedder_policy),
      host_receiver_(this, std::move(host)),
      coep_reporter_(std::move(coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(worker_process_host_);
  DCHECK(worker_process_host_->IsInitializedAndNotDead());
  DCHECK(coep_reporter_);
  DCHECK((creator_render_frame_host_id_ && !creator_worker_token_) ||
         (!creator_render_frame_host_id_ && creator_worker_token_));

  scoped_process_host_observation_.Observe(worker_process_host_);

  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    // This is a workaround to make the worker's COEP have a value when
    // PlzDedicatedWorker is disabled. When the feature is enabled, The value is
    // initialized in DedicatedWorkerHost::DidStartScriptLoad().
    worker_cross_origin_embedder_policy_ =
        creator_cross_origin_embedder_policy_;
  }

  service_->NotifyWorkerCreated(this);
}

DedicatedWorkerHost::~DedicatedWorkerHost() {
  service_->NotifyBeforeWorkerDestroyed(token_, ancestor_render_frame_host_id_);
}

void DedicatedWorkerHost::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
  broker_receiver_.set_disconnect_handler(base::BindOnce(
      &DedicatedWorkerHost::OnMojoDisconnect, base::Unretained(this)));
}

void DedicatedWorkerHost::CreateContentSecurityNotifier(
    mojo::PendingReceiver<blink::mojom::ContentSecurityNotifier> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ContentSecurityNotifier>(ancestor_render_frame_host_id_),
      std::move(receiver));
}

void DedicatedWorkerHost::OnMojoDisconnect() {
  delete this;
}

void DedicatedWorkerHost::RenderProcessExited(
    RenderProcessHost* render_process_host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(worker_process_host_, render_process_host);

  delete this;
}

void DedicatedWorkerHost::StartScriptLoad(
    const GURL& script_url,
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

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  // Get nearest ancestor render frame host in order to determine the
  // top-frame origin to use for the network isolation key.
  RenderFrameHostImpl* nearest_ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!nearest_ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (script_url.SchemeIsBlob()) {
    if (!blob_url_token) {
      mojo::ReportBadMessage("DWH_NO_BLOB_URL_TOKEN");
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            storage_partition_impl, std::move(blob_url_token));
  } else if (blob_url_token) {
    mojo::ReportBadMessage("DWH_NOT_BLOB_URL");
    return;
  }

  // If this is a nested worker, there is no creator frame.
  RenderFrameHostImpl* creator_render_frame_host = nullptr;
  if (creator_render_frame_host_id_) {
    creator_render_frame_host =
        RenderFrameHostImpl::FromID(creator_render_frame_host_id_.value());
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
    auto* appcache_service = storage_partition_impl->GetAppCacheService();
    if (appcache_service) {
      appcache_host =
          appcache_service->GetHost(appcache_handle->appcache_host_id());
    }
  }

  // Set if the subresource loader factories support file URLs so that we can
  // recreate the factories after Network Service crashes.
  // TODO(nhiroki): Currently this flag is calculated based on the request
  // initiator origin to keep consistency with WorkerScriptFetchInitiator, but
  // probably this should be calculated based on the worker origin as the
  // factories be used for subresource loading on the worker.
  file_url_support_ = creator_origin_.scheme() == url::kFileScheme;

  service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
      storage_partition_impl->GetServiceWorkerContext(), base::DoNothing());

  // For blob URL workers, inherit the controller from the worker's parent.
  // See https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
  if (script_url.SchemeIsBlob()) {
    if (creator_render_frame_host_id_) {
      // The creator of this worker is a frame.
      base::WeakPtr<ServiceWorkerContainerHost> creator_container_host =
          RenderFrameHostImpl::FromID(creator_render_frame_host_id_.value())
              ->GetLastCommittedServiceWorkerHost();

      service_worker_handle_->set_parent_container_host(creator_container_host);
    } else {
      // The creator of this worker is a dedicated worker.
      DCHECK(creator_worker_token_);

      DedicatedWorkerHost* creator_worker =
          service_->GetDedicatedWorkerHostFromToken(
              creator_worker_token_.value());
      if (!creator_worker) {
        client_->OnScriptLoadStartFailed();
        return;
      }

      base::WeakPtr<ServiceWorkerContainerHost> creator_container_host =
          creator_worker->service_worker_handle()->container_host();

      service_worker_handle_->set_parent_container_host(creator_container_host);
    }
  }

  // Get a storage domain.
  auto partition_domain =
      nearest_ancestor_render_frame_host->GetSiteInstance()->GetPartitionDomain(
          storage_partition_impl);

  WorkerScriptFetchInitiator::Start(
      worker_process_host_->GetID(), token_, script_url,
      creator_render_frame_host,
      nearest_ancestor_render_frame_host->ComputeSiteForCookies(),
      creator_origin_,
      nearest_ancestor_render_frame_host->GetIsolationInfoForSubresources(),
      credentials_mode, std::move(outside_fetch_client_settings_object),
      network::mojom::RequestDestination::kWorker,
      storage_partition_impl->GetServiceWorkerContext(),
      service_worker_handle_.get(),
      appcache_host ? appcache_host->GetWeakPtr() : nullptr,
      std::move(blob_url_loader_factory), nullptr, storage_partition_impl,
      partition_domain,
      // TODO(crbug.com/1138622): Propagate dedicated worker ukm::SourceId here.
      ukm::kInvalidSourceId,
      // TODO(crbug.com/1143102): pass DevToolsAgentHostImpl for the worker.
      nullptr, base::UnguessableToken(),
      base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the dedicated worker scope");
}

void DedicatedWorkerHost::DidStartScriptLoad(
    bool success,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    const GURL& final_response_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  if (!success) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // TODO(https://crbug.com/986188): Check if the main script's final response
  // URL is committable.
  final_response_url_ = final_response_url;
  service_->NotifyWorkerFinalResponseURLDetermined(token_, final_response_url);

  // TODO(cammie): Change this approach when we support shared workers
  // creating dedicated workers, as there might be no ancestor frame.
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // https://html.spec.whatwg.org/C/#run-a-worker
  worker_cross_origin_embedder_policy_ = network::CrossOriginEmbedderPolicy();
  if (final_response_url.SchemeIsBlob() ||
      final_response_url.SchemeIs(url::kAboutScheme) ||
      final_response_url.SchemeIs(url::kDataScheme)) {
    // > 14.5 If response's url's scheme is a local scheme, then set worker
    // global scope's embedder policy to owner's embedder policy.
    worker_cross_origin_embedder_policy_ =
        creator_cross_origin_embedder_policy_;
  } else if (main_script_load_params->response_head->parsed_headers) {
    // > 14.6 Otherwise, set worker global scope's embedder policy to the result
    // of obtaining an embedder policy from response.
    worker_cross_origin_embedder_policy_ =
        main_script_load_params->response_head->parsed_headers
            ->cross_origin_embedder_policy;
  }

  // > 14.8 If the result of checking a global object's embedder policy with
  // worker global scope, owner, and response is false, then set response to a
  // network error.
  if (!CheckCrossOriginEmbedderPolicy(creator_cross_origin_embedder_policy_,
                                      cross_origin_embedder_policy())) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // Start observing Network Service crash when it's running out-of-process.
  if (IsOutOfProcessNetworkService()) {
    ObserveNetworkServiceCrash(static_cast<StoragePartitionImpl*>(
        worker_process_host_->GetStoragePartition()));
  }

  // Set up the default network loader factory.
  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
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
      service_worker_handle_->TakeContainerInfo(),
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
    RenderFrameHostImpl* ancestor_render_frame_host,
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ancestor_render_frame_host);
  DCHECK(bypass_redirect_checks);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  DCHECK(coep_reporter_);
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForFrame(
          ancestor_render_frame_host, worker_origin_, isolation_info_,
          ancestor_render_frame_host->BuildClientSecurityState(),
          std::move(coep_reporter), worker_process_host_,
          ancestor_render_frame_host->IsFeatureEnabled(
              blink::mojom::FeaturePolicyFeature::kTrustTokenRedemption)
              ? network::mojom::TrustTokenRedemptionPolicy::kPotentiallyPermit
              : network::mojom::TrustTokenRedemptionPolicy::kForbid,
          "DedicatedWorkerHost::CreateNetworkFactoryForSubresources");
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      worker_process_host_->GetBrowserContext(),
      /*frame=*/nullptr, worker_process_host_->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource,
      worker_origin_, /*navigation_id=*/base::nullopt,
      ukm::SourceIdObj::FromInt64(
          ancestor_render_frame_host->GetPageUkmSourceId()),
      &default_factory_receiver, &factory_params->header_client,
      bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, &factory_params->factory_override);

  // TODO(nhiroki): Call devtools_instrumentation::WillCreateURLLoaderFactory()
  // here.

  worker_process_host_->CreateURLLoaderFactory(
      std::move(default_factory_receiver), std::move(factory_params));

  return pending_default_factory;
}

// The implementation of the following algorithm:
// https://html.spec.whatwg.org/C/#check-a-global-object's-embedder-policy
bool DedicatedWorkerHost::CheckCrossOriginEmbedderPolicy(
    network::CrossOriginEmbedderPolicy creator_cross_origin_embedder_policy,
    network::CrossOriginEmbedderPolicy worker_cross_origin_embedder_policy) {
  // > 4. If ownerPolicy's report-only value is "require-corp" and policy's
  // value is "unsafe-none", then queue a cross-origin embedder policy
  // inheritance violation with response, "worker initialization", owner's
  // policy's report only reporting endpoint, "reporting", and owner.
  // TODO(crbug.com/1060837): Queue a report if the report-only value is not
  // valid.

  // > 5. If ownerPolicy's value is "unsafe-none" or policy's value is
  // "require-corp", then return true.
  if (creator_cross_origin_embedder_policy.value ==
          network::mojom::CrossOriginEmbedderPolicyValue::kNone ||
      worker_cross_origin_embedder_policy.value ==
          network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp) {
    return true;
  }

  // > 6. Queue a cross-origin embedder policy inheritance violation with
  // response, "worker initialization", owner's policy's reporting endpoint,
  // "enforce", and owner.
  // TODO(crbug.com/1060837): Queue this report.

  // > 7. Return false.
  return false;
}

void DedicatedWorkerHost::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
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
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    receiver.ResetWithReason(0, "The parent frame has already been gone.");
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          ancestor_render_frame_host_id_.child_id,
          ancestor_render_frame_host_id_.frame_routing_id, worker_origin_,
          ancestor_render_frame_host->GetIsolationInfoForSubresources()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          worker_process_host_->GetID(), /*frame=*/nullptr, worker_origin_,
          isolation_info_.network_isolation_key()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateWakeLockService(
    mojo::PendingReceiver<blink::mojom::WakeLockService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Unconditionally disallow wake locks from workers until
  // WakeLockPermissionContext has been updated to no longer force the
  // permission to "denied" and WakeLockServiceImpl checks permissions on
  // every request.
  return;
}

void DedicatedWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());
  worker_process_host_->BindCacheStorage(cross_origin_embedder_policy(),
                                         std::move(coep_reporter),
                                         worker_origin_, std::move(receiver));
}

void DedicatedWorkerHost::CreateNestedDedicatedWorker(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());
  // Set this worker as the creator of the new worker and inherit the ancestor
  // render frame.

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_host_->GetID(),
          /*creator_render_frame_host_id_=*/base::nullopt,
          /*creator_worker_token=*/token_, ancestor_render_frame_host_id_,
          worker_origin_, isolation_info_, cross_origin_embedder_policy(),
          std::move(coep_reporter)),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindIdleManager(std::move(receiver));
}

void DedicatedWorkerHost::BindWebOTPServiceReceiver(
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindWebOTPServiceReceiver(std::move(receiver));
}

#if !defined(OS_ANDROID)
void DedicatedWorkerHost::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
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
  params->process_id = worker_process_host_->GetID();
  params->debug_tag = "DedicatedWorkerHost::ObserveNetworkServiceCrash";
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

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host)
    return;

  // Get a storage domain.
  auto partition_domain =
      ancestor_render_frame_host->GetSiteInstance()->GetPartitionDomain(
          storage_partition_impl);

  // Start observing Network Service crash again.
  ObserveNetworkServiceCrash(storage_partition_impl);

  // If this is a nested worker, there is no creator frame and
  // |creator_render_frame_host| will be null.
  RenderFrameHostImpl* creator_render_frame_host =
      creator_render_frame_host_id_
          ? RenderFrameHostImpl::FromID(creator_render_frame_host_id_.value())
          : nullptr;

  // Recreate the default URLLoaderFactory. This doesn't support
  // AppCache-specific factory.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories =
          WorkerScriptFetchInitiator::CreateFactoryBundle(
              WorkerScriptFetchInitiator::LoaderType::kSubResource,
              worker_process_host_->GetID(), storage_partition_impl,
              partition_domain, file_url_support_,
              /*filesystem_url_support=*/true, creator_render_frame_host);

  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  subresource_loader_updater_->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

}  // namespace content
