// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/loader/content_security_notifier.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/web_transport_connector_impl.h"
#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"
#include "content/browser/worker_host/dedicated_worker_hosts_for_document.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#endif

namespace content {

DedicatedWorkerHost::DedicatedWorkerHost(
    DedicatedWorkerServiceImpl* service,
    const blink::DedicatedWorkerToken& token,
    RenderProcessHost* worker_process_host,
    absl::optional<GlobalRenderFrameHostId> creator_render_frame_host_id,
    absl::optional<blink::DedicatedWorkerToken> creator_worker_token,
    GlobalRenderFrameHostId ancestor_render_frame_host_id,
    const blink::StorageKey& creator_storage_key,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr creator_client_security_state,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> ancestor_coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host)
    : service_(service),
      token_(token),
      worker_process_host_(worker_process_host),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      creator_worker_token_(creator_worker_token),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_storage_key.origin()),
      // TODO(https://crbug.com/1058759): Calculate the worker origin based on
      // the worker script URL (the worker's storage key should have an opaque
      // origin if the worker script URL's scheme is data:).
      storage_key_(creator_storage_key),
      isolation_info_(isolation_info),
      reporting_source_(base::UnguessableToken::Create()),
      creator_client_security_state_(std::move(creator_client_security_state)),
      host_receiver_(this, std::move(host)),
      creator_coep_reporter_(std::move(creator_coep_reporter)),
      ancestor_coep_reporter_(std::move(ancestor_coep_reporter)),
      code_cache_host_receivers_(GetProcessHost()
                                     ->GetStoragePartition()
                                     ->GetGeneratedCodeCacheContext()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(worker_process_host_);
  DCHECK(worker_process_host_->IsInitializedAndNotDead());
  DCHECK_NE(creator_render_frame_host_id_.has_value(),
            creator_worker_token_.has_value());
  DCHECK(creator_client_security_state_);

  // TODO(https://crbug.com/11990077): Once we add more stuff to
  // `blink::StorageKey`, DCHECK that `storage_key` is consistent with
  // `isolation_info_` here (i.e. their origin and top frame origin match).

  scoped_process_host_observation_.Observe(worker_process_host_.get());

  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    // This is a workaround to make the worker's COEP have a value when
    // PlzDedicatedWorker is disabled. When the feature is enabled, The value is
    // initialized in DedicatedWorkerHost::DidStartScriptLoad().
    worker_client_security_state_ = creator_client_security_state_->Clone();
  }

  service_->NotifyWorkerCreated(this);

  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (ancestor_render_frame_host) {
    DedicatedWorkerHostsForDocument::GetOrCreateForCurrentDocument(
        ancestor_render_frame_host)
        ->Add(weak_factory_.GetSafeRef());
  }
}

DedicatedWorkerHost::~DedicatedWorkerHost() {
  // This DedicatedWorkerHost is destroyed via either the mojo disconnection
  // or RenderProcessHostObserver. This destruction should be called before
  // the observed render process host (`worker_process_host_`) is destroyed.

  // The frame's current document might no longer be related to this worker. In
  // this case, the previous DedicatedWorkerHostsForDocument has been deleted
  // and calling Remove(...)` on the new one is a no-op. Note that when the
  // previous document is BFCached and not deleted, the RenderFrameHost will
  // never be reused, so we will always get the right (BFCached) document.
  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (ancestor_render_frame_host) {
    DedicatedWorkerHostsForDocument::GetOrCreateForCurrentDocument(
        ancestor_render_frame_host)
        ->Remove(weak_factory_.GetSafeRef());
  }

  // Send any final reports and allow the reporting configuration to be
  // removed. Note that the RenderProcessHost and the associated
  // StoragePartition outlives `this`.
  worker_process_host_->GetStoragePartition()
      ->GetNetworkContext()
      ->SendReportsAndRemoveSource(reporting_source_);

  service_->NotifyBeforeWorkerDestroyed(token_, ancestor_render_frame_host_id_);

  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker))
    WorkerDevToolsManager::GetInstance().WorkerDestroyed(this);
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

void DedicatedWorkerHost::RenderProcessHostDestroyed(
    RenderProcessHost* render_process_host) {
  DCHECK_EQ(worker_process_host_, render_process_host);

  // In --single-process mode, RenderProcessExited() is not called, so we must
  // also listen to RenderProcessHostDestroyed() to know to delete `this` and
  // preserve the invariant that RenderProcessHostImpl outlives `this`.
  DCHECK(RenderProcessHost::run_renderer_in_process());

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

  // Get nearest ancestor RenderFrameHost in order to determine the
  // top-frame origin to use for the network isolation key.
  RenderFrameHostImpl* nearest_ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!nearest_ancestor_render_frame_host) {
    ScriptLoadStartFailed(script_url,
                          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
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

  RenderFrameHostImpl* creator_render_frame_host = nullptr;
  DedicatedWorkerHost* creator_worker = nullptr;
  if (creator_render_frame_host_id_) {
    // This is not a nested worker, it has a creator frame.
    creator_render_frame_host =
        RenderFrameHostImpl::FromID(*creator_render_frame_host_id_);
    if (!creator_render_frame_host) {
      ScriptLoadStartFailed(
          script_url, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
      return;
    }
  } else {
    // The creator of this worker is a dedicated worker.
    DCHECK(creator_worker_token_);

    creator_worker =
        service_->GetDedicatedWorkerHostFromToken(*creator_worker_token_);
    if (!creator_worker) {
      ScriptLoadStartFailed(
          script_url, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
      return;
    }
  }

  // At this point there is either a creator frame or a creator worker.
  //
  // This may change at some point in the future if dedicated workers can be
  // nested inside shared workers, as the HTML spec dictates. For now, nesting
  // is only supported for dedicated workers inside dedicated workers, so the
  // following invariant holds. If and when this changes, conditionals below
  // should be revisited to account for the novel possibility of a creator
  // shared worker.
  DCHECK_NE(creator_render_frame_host == nullptr, creator_worker == nullptr);

  // Set if the subresource loader factories support file URLs so that we can
  // recreate the factories after Network Service crashes.
  // TODO(nhiroki): Currently this flag is calculated based on the request
  // initiator origin to keep consistency with WorkerScriptFetcher, but probably
  // this should be calculated based on the worker origin as the factories be
  // used for subresource loading on the worker.
  file_url_support_ = creator_origin_.scheme() == url::kFileScheme;

  service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
      storage_partition_impl->GetServiceWorkerContext(), base::DoNothing());

  // For blob URL workers, inherit the controller from the worker's parent.
  // See https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
  if (script_url.SchemeIsBlob()) {
    if (creator_render_frame_host) {
      // The creator of this worker is a frame.
      service_worker_handle_->set_parent_container_host(
          creator_render_frame_host->GetLastCommittedServiceWorkerHost());
    } else {
      base::WeakPtr<ServiceWorkerContainerHost> creator_container_host =
          creator_worker->service_worker_handle()->container_host();
      service_worker_handle_->set_parent_container_host(creator_container_host);
    }
  }

  network::mojom::ClientSecurityStatePtr client_security_state;
  if (creator_render_frame_host) {
    client_security_state =
        creator_render_frame_host->BuildClientSecurityStateForWorkers();
  } else {
    client_security_state = creator_worker->client_security_state()->Clone();
  }

  // Get a storage domain.
  auto partition_domain =
      nearest_ancestor_render_frame_host->GetSiteInstance()->GetPartitionDomain(
          storage_partition_impl);

  WorkerScriptFetcher::CreateAndStart(
      worker_process_host_->GetID(), token_, script_url,
      nearest_ancestor_render_frame_host, creator_render_frame_host,
      nearest_ancestor_render_frame_host->ComputeSiteForCookies(),
      creator_origin_, storage_key_,
      nearest_ancestor_render_frame_host->GetIsolationInfoForSubresources(),
      std::move(client_security_state), credentials_mode,
      std::move(outside_fetch_client_settings_object),
      network::mojom::RequestDestination::kWorker,
      storage_partition_impl->GetServiceWorkerContext(),
      service_worker_handle_.get(), std::move(blob_url_loader_factory), nullptr,
      storage_partition_impl, partition_domain,
      // TODO(crbug.com/1138622): Propagate dedicated worker ukm::SourceId here.
      ukm::kInvalidSourceId, WorkerDevToolsAgentHost::GetFor(this),
      token_.value(),
      base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the dedicated worker scope");
}

void DedicatedWorkerHost::DidStartScriptLoad(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    const GURL& final_response_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  if (!main_script_load_params) {
    ScriptLoadStartFailed(final_response_url,
                          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
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
    ScriptLoadStartFailed(final_response_url,
                          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // https://html.spec.whatwg.org/C/#run-a-worker
  if (final_response_url.SchemeIsLocal()) {
    // TODO(https://crbug.com/1146362): Inherit from the file creator instead
    // once creator policies are persisted through the filesystem store.
    if (base::FeatureList::IsEnabled(
            features::kPrivateNetworkAccessForWorkers)) {
      worker_client_security_state_ = creator_client_security_state_->Clone();
    } else {
      // Preserve incorrect functionality if PNA is not enabled.
      worker_client_security_state_ =
          ancestor_render_frame_host->BuildClientSecurityState();

      // > 14.5 If response's url's scheme is a local scheme, then set worker
      // global scope's embedder policy to owner's embedder policy.
      worker_client_security_state_->cross_origin_embedder_policy =
          creator_client_security_state_->cross_origin_embedder_policy;
    }
  } else if (main_script_load_params) {
    DCHECK(main_script_load_params->response_head);
    DCHECK(main_script_load_params->response_head->parsed_headers);

    if (base::FeatureList::IsEnabled(
            features::kPrivateNetworkAccessForWorkers)) {
      worker_client_security_state_ =
          network::mojom::ClientSecurityState::New();
      worker_client_security_state_->ip_address_space = CalculateIPAddressSpace(
          final_response_url, main_script_load_params->response_head.get(),
          GetContentClient()->browser());
      worker_client_security_state_->is_web_secure_context =
          network::IsUrlPotentiallyTrustworthy(final_response_url) &&
          creator_client_security_state_->is_web_secure_context;
      worker_client_security_state_->private_network_request_policy =
          DerivePrivateNetworkRequestPolicy(
              worker_client_security_state_->ip_address_space,
              worker_client_security_state_->is_web_secure_context,
              PrivateNetworkRequestContext::kWorker);
    } else {
      // Preserve incorrect functionality if PNA is not enabled.
      worker_client_security_state_ =
          ancestor_render_frame_host->BuildClientSecurityState();
    }

    // > 14.6 Otherwise, set worker global scope's embedder policy to the result
    // of obtaining an embedder policy from response.
    worker_client_security_state_->cross_origin_embedder_policy =
        main_script_load_params->response_head->parsed_headers
            ->cross_origin_embedder_policy;
  }

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  // Create a COEP reporter with worker's policy.
  const network::CrossOriginEmbedderPolicy& coep =
      worker_client_security_state_->cross_origin_embedder_policy;
  coep_reporter_ = std::make_unique<CrossOriginEmbedderPolicyReporter>(
      storage_partition->GetWeakPtr(), final_response_url,
      coep.reporting_endpoint, coep.report_only_reporting_endpoint,
      reporting_source_, isolation_info_.network_anonymization_key());
  // TODO(crbug.com/1197041): Bind the receiver of ReportingObserver to the
  // worker in the renderer process.

  // > 14.8 If the result of checking a global object's embedder policy with
  // worker global scope, owner, and response is false, then set response to a
  // network error.
  if (!CheckCrossOriginEmbedderPolicy()) {
    ScriptLoadStartFailed(final_response_url,
                          network::URLLoaderCompletionStatus(
                              network::mojom::BlockedByResponseReason::
                                  kCoepFrameResourceNeedsCoepHeader));
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

  // Notify that the loading is completed to DevTools. It fires
  // `Network.onLoadingFinished` event.
  devtools_instrumentation::OnWorkerMainScriptLoadingFinished(
      FrameTreeNode::From(ancestor_render_frame_host),
      WorkerDevToolsAgentHost::GetFor(this)->devtools_worker_token(),
      network::URLLoaderCompletionStatus(net::OK));

  client_->OnScriptLoadStarted(
      service_worker_handle_->TakeContainerInfo(),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories),
      subresource_loader_updater_.BindNewPipeAndPassReceiver(),
      std::move(controller),
      BindAndPassRemoteForBackForwardCacheControllerHost());

  // |service_worker_remote_object| is an associated remote, so calls can't be
  // made on it until its receiver is sent. Now that the receiver was sent, it
  // can be used, so add it to ServiceWorkerObjectHost.
  if (service_worker_remote_object) {
    controller_service_worker_object_host->AddRemoteObjectPtrAndUpdateState(
        std::move(service_worker_remote_object), service_worker_state);
  }
}

void DedicatedWorkerHost::ScriptLoadStartFailed(
    const GURL& url,
    const network::URLLoaderCompletionStatus& status) {
  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  // Notify that the loading failed to DevTools. It fires
  // `Network.onLoadingFailed` event.
  devtools_instrumentation::OnWorkerMainScriptLoadingFailed(
      url, WorkerDevToolsAgentHost::GetFor(this)->devtools_worker_token(),
      FrameTreeNode::From(ancestor_render_frame_host),
      ancestor_render_frame_host, status);

  client_->OnScriptLoadStartFailed();
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DedicatedWorkerHost::CreateNetworkFactoryForSubresources(
    RenderFrameHostImpl* ancestor_render_frame_host,
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ancestor_render_frame_host);
  DCHECK(bypass_redirect_checks);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  if (GetWorkerCoepReporter()) {
    GetWorkerCoepReporter()->Clone(
        coep_reporter.InitWithNewPipeAndPassReceiver());
  }

  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForFrame(
          ancestor_render_frame_host, GetStorageKey().origin(), isolation_info_,
          worker_client_security_state_->Clone(), std::move(coep_reporter),
          worker_process_host_,
          ancestor_render_frame_host->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::
                  kPrivateStateTokenIssuance)
              ? network::mojom::TrustTokenOperationPolicyVerdict::
                    kPotentiallyPermit
              : network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
          ancestor_render_frame_host->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::kTrustTokenRedemption)
              ? network::mojom::TrustTokenOperationPolicyVerdict::
                    kPotentiallyPermit
              : network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
          ancestor_render_frame_host->GetCookieSettingOverrides(),
          "DedicatedWorkerHost::CreateNetworkFactoryForSubresources");
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      worker_process_host_->GetBrowserContext(),
      /*frame=*/nullptr, worker_process_host_->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource,
      GetStorageKey().origin(), /*navigation_id=*/absl::nullopt,
      ukm::SourceIdObj::FromInt64(
          ancestor_render_frame_host->GetPageUkmSourceId()),
      &default_factory_receiver, &factory_params->header_client,
      bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, &factory_params->factory_override,
      /*navigation_response_task_runner=*/nullptr);

  devtools_instrumentation::WillCreateURLLoaderFactory(
      ancestor_render_frame_host, /*is_navigation=*/false,
      /*is_download=*/false, &default_factory_receiver,
      &factory_params->factory_override);

  worker_process_host_->CreateURLLoaderFactory(
      std::move(default_factory_receiver), std::move(factory_params));

  return pending_default_factory;
}

// [spec]
// https://html.spec.whatwg.org/C/#check-a-global-object's-embedder-policy
bool DedicatedWorkerHost::CheckCrossOriginEmbedderPolicy() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(final_response_url_);

  if (!creator_coep_reporter_)
    return false;

  const network::CrossOriginEmbedderPolicy&
      creator_cross_origin_embedder_policy =
          creator_client_security_state_->cross_origin_embedder_policy;
  const network::CrossOriginEmbedderPolicy&
      worker_cross_origin_embedder_policy = cross_origin_embedder_policy();

  // [spec]: 4. If ownerPolicy's report-only value is "require-corp" or
  // "credentialless" and policy's value is "unsafe-none", then queue a
  // cross-origin embedder policy inheritance violation with response, "worker
  // initialization", owner's policy's report only reporting endpoint,
  // "reporting", and owner.
  if (network::CompatibleWithCrossOriginIsolated(
          creator_cross_origin_embedder_policy.report_only_value) &&
      !network::CompatibleWithCrossOriginIsolated(
          worker_cross_origin_embedder_policy)) {
    creator_coep_reporter_->QueueWorkerInitializationReport(
        final_response_url_.value(),
        /*report_only=*/true);
  }

  // [spec]: 5. If ownerPolicy's value is "unsafe-none" or policy's value is
  // "require-corp" or "credentialless", then return true.
  if (!network::CompatibleWithCrossOriginIsolated(
          creator_cross_origin_embedder_policy) ||
      network::CompatibleWithCrossOriginIsolated(
          worker_cross_origin_embedder_policy)) {
    return true;
  }

  // [spec]: 6. Queue a cross-origin embedder policy inheritance violation with
  // response, "worker initialization", owner's policy's reporting endpoint,
  // "enforce", and owner.
  creator_coep_reporter_->QueueWorkerInitializationReport(
      final_response_url_.value(),
      /*report_only=*/false);

  // [spec]: 7. Return false.
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
void DedicatedWorkerHost::CreateDirectSocketsService(
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  // The ancestor frame may have already been closed. In that case, the worker
  // will soon be terminated too, so abort the connection.
  if (!ancestor_render_frame_host) {
    return;
  }

  DirectSocketsServiceImpl::CreateForFrame(ancestor_render_frame_host,
                                           std::move(receiver));
}
#endif

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
          ancestor_render_frame_host_id_.frame_routing_id,
          GetStorageKey().origin(),
          ancestor_render_frame_host->GetIsolationInfoForSubresources()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateWebTransportConnector(
    mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver) {
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
      std::make_unique<WebTransportConnectorImpl>(
          worker_process_host_->GetID(),
          ancestor_render_frame_host->GetWeakPtr(), GetStorageKey().origin(),
          isolation_info_.network_anonymization_key()),
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
  BindCacheStorageInternal(
      std::move(receiver),
      storage::BucketLocator::ForDefaultBucket(GetStorageKey()));
}

void DedicatedWorkerHost::CreateNestedDedicatedWorker(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // For the non-PlzDedicatedWorker case, use ancestor's COEP reporter as a
  // `creator_coep_reporter` to keep the current behavior, but it's not aligned
  // with the spec.
  base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter =
      GetWorkerCoepReporter();

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_host_->GetID(),
          /*creator_render_frame_host_id_=*/absl::nullopt,
          /*creator_worker_token=*/token_, ancestor_render_frame_host_id_,
          GetStorageKey(), isolation_info_,
          worker_client_security_state_->Clone(), creator_coep_reporter,
          ancestor_coep_reporter_),
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

void DedicatedWorkerHost::CreateBroadcastChannelProvider(
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      GetProcessHost()->GetStoragePartition());

  auto* broadcast_channel_service =
      storage_partition_impl->GetBroadcastChannelService();
  broadcast_channel_service->AddReceiver(
      std::make_unique<BroadcastChannelProvider>(broadcast_channel_service,
                                                 GetStorageKey()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateBlobUrlStoreProvider(
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      GetProcessHost()->GetStoragePartition());

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      GetStorageKey(), std::move(receiver),
      storage::BlobURLValidityCheckBehavior::
          ALLOW_OPAQUE_ORIGIN_STORAGE_KEY_MISMATCH);
}

void DedicatedWorkerHost::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  RenderProcessHost* rph = GetProcessHost();
  code_cache_host_receivers_.Add(rph->GetID(),
                                 isolation_info_.network_isolation_key(),
                                 GetStorageKey(), std::move(receiver));
}

#if !BUILDFLAG(IS_ANDROID)
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

void DedicatedWorkerHost::CreateBucketManagerHost(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  GetProcessHost()->BindBucketManagerHost(GetWeakPtr(), std::move(receiver));
}

void DedicatedWorkerHost::ObserveNetworkServiceCrash(
    StoragePartitionImpl* storage_partition_impl) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

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
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

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

  // Recreate the default URLLoaderFactory.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories = WorkerScriptFetcher::CreateFactoryBundle(
          WorkerScriptFetcher::LoaderType::kSubResource,
          worker_process_host_->GetID(), storage_partition_impl,
          partition_domain, file_url_support_,
          /*filesystem_url_support=*/true, creator_render_frame_host,
          storage_key_);

  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  subresource_loader_updater_->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

void DedicatedWorkerHost::MaybeCountWebFeature(const GURL& script_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host)
    return;

  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      ancestor_render_frame_host->GetLastCommittedServiceWorkerHost();
  if (!container_host || !container_host->controller())
    return;

  if (!blink::ServiceWorkerScopeMatches(container_host->controller()->scope(),
                                        script_url) ||
      container_host->key() != storage_key_) {
    // Count the number of dedicated workers that 1) are controlled by a service
    // worker that is inherited from a controlled document, and 2) will not be
    // controlled by that service worker after PlzDedicatedWorker is enabled.
    container_host->CountFeature(
        blink::mojom::WebFeature::kWorkerControlledByServiceWorkerOutOfScope);

    DCHECK_NE(container_host->controller()->fetch_handler_existence(),
              ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);
    if (container_host->controller()->fetch_handler_existence() ==
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
      // Count the number of dedicated workers that 1) are controlled by a
      // service worker that is inherited from a controlled document, 2) will
      // not be controlled by that service worker after PlzDedicatedWorker is
      // enabled, and 3) have a fetch event handler.
      // `kControlledWorkerWillBeUncontrolled` excludes the cases if a
      // dedicated worker is controlled by any registered service worker.
      container_host->CountFeature(
          blink::mojom::WebFeature::
              kWorkerControlledByServiceWorkerWithFetchEventHandlerOutOfScope);

      ServiceWorkerContextWrapper* service_worker_context =
          static_cast<StoragePartitionImpl*>(
              worker_process_host_->GetStoragePartition())
              ->GetServiceWorkerContext();
      if (!service_worker_context)
        return;

      service_worker_context->GetRegistrationsForStorageKey(
          blink::StorageKey::CreateFirstParty(
              ancestor_render_frame_host->GetLastCommittedOrigin()),
          base::BindOnce(&DedicatedWorkerHost::ContinueOnMaybeCountWebFeature,
                         weak_factory_.GetWeakPtr(), script_url,
                         std::move(container_host)));
    }
  }
}

void DedicatedWorkerHost::ContinueOnMaybeCountWebFeature(
    const GURL& script_url,
    base::WeakPtr<ServiceWorkerContainerHost> ancestor_container_host,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  DCHECK(!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  if (!ancestor_container_host || status != blink::ServiceWorkerStatusCode::kOk)
    return;

  for (const auto& registration : registrations) {
    // Do not record the UseCounter because a dedicated worker is in scope of
    // one of service workers registered for the origin. The scope matched
    // service worker may be different from the one that controls the ancestor
    // frame.
    if (blink::ServiceWorkerScopeMatches(registration->scope(), script_url) &&
        registration->key() == storage_key_)
      return;
  }

  // Count the number of dedicated workers that are not controlled by any
  // service worker registered for the origin after PlzDedicatedWorker is
  // enabled.
  ancestor_container_host->CountFeature(
      blink::mojom::WebFeature::kControlledWorkerWillBeUncontrolled);

  // Exclude the cases that `script_url` is a blob URL from
  // kControlledWorkerWillBeUncontrolled.
  if (!script_url.SchemeIsBlob()) {
    ancestor_container_host->CountFeature(
        blink::mojom::WebFeature::
            kControlledNonBlobURLWorkerWillBeUncontrolled);
  }
}

base::WeakPtr<CrossOriginEmbedderPolicyReporter>
DedicatedWorkerHost::GetWorkerCoepReporter() {
  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    DCHECK(coep_reporter_);
    return coep_reporter_->GetWeakPtr();
  }
  // For the non-PlzDedicatedWorker case, use ancestor's COEP reporter to keep
  // the current behavior, but it's not aligned with the spec.
  // `ancestor_coep_reporter_` is possible to be nullptr, which means the
  // ancestor render frame has already been closed or navigated and this worker
  // will also be terminated soon.
  return ancestor_coep_reporter_;
}

void DedicatedWorkerHost::EvictFromBackForwardCache(
    blink::mojom::RendererEvictionReason reason) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The frame may have already been closed.
    return;
  }
  ancestor_render_frame_host->EvictFromBackForwardCache(reason);
}

void DedicatedWorkerHost::DidChangeBackForwardCacheDisablingFeatures(
    BackForwardCacheBlockingDetails details) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The frame may have already been closed.
    return;
  }
  bfcache_blocking_details_ = std::move(details);
  ancestor_render_frame_host->MaybeEvictFromBackForwardCache();
}

blink::StorageKey DedicatedWorkerHost::GetBucketStorageKey() {
  return GetStorageKey();
}

blink::mojom::PermissionStatus DedicatedWorkerHost::GetPermissionStatus(
    blink::PermissionType permission_type) {
  return GetProcessHost()
      ->GetBrowserContext()
      ->GetPermissionController()
      ->GetPermissionStatusForWorker(permission_type, GetProcessHost(),
                                     GetStorageKey().origin());
}

void DedicatedWorkerHost::BindCacheStorageForBucket(
    const storage::BucketInfo& bucket,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BindCacheStorageInternal(std::move(receiver), bucket.ToBucketLocator());
}

void DedicatedWorkerHost::BindCacheStorageInternal(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
    const storage::BucketLocator& bucket_locator) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  if (GetWorkerCoepReporter()) {
    GetWorkerCoepReporter()->Clone(
        coep_reporter.InitWithNewPipeAndPassReceiver());
  }
  worker_process_host_->BindCacheStorage(cross_origin_embedder_policy(),
                                         std::move(coep_reporter),
                                         bucket_locator, std::move(receiver));
}

void DedicatedWorkerHost::GetSandboxedFileSystemForBucket(
    const storage::BucketInfo& bucket,
    blink::mojom::BucketHost::GetDirectoryCallback callback) {
  GetProcessHost()->GetSandboxedFileSystemForBucket(bucket.ToBucketLocator(),
                                                    std::move(callback));
}

GlobalRenderFrameHostId DedicatedWorkerHost::GetAssociatedRenderFrameHostId()
    const {
  return GetAncestorRenderFrameHostId();
}

blink::scheduler::WebSchedulerTrackedFeatures
DedicatedWorkerHost::GetBackForwardCacheDisablingFeatures() const {
  blink::scheduler::WebSchedulerTrackedFeatures features;
  for (auto& details : bfcache_blocking_details_) {
    features.Put(static_cast<blink::scheduler::WebSchedulerTrackedFeature>(
        details->feature));
  }
  return features;
}

const DedicatedWorkerHost::BackForwardCacheBlockingDetails&
DedicatedWorkerHost::GetBackForwardCacheBlockingDetails() const {
  return bfcache_blocking_details_;
}

base::WeakPtr<ServiceWorkerContainerHost>
DedicatedWorkerHost::GetServiceWorkerContainerHost() {
  if (!service_worker_handle_) {
    return nullptr;
  }
  return service_worker_handle_->container_host();
}

mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
DedicatedWorkerHost::BindAndPassRemoteForBackForwardCacheControllerHost() {
  return back_forward_cache_controller_host_receiver_
      .BindNewPipeAndPassRemote();
}

}  // namespace content
