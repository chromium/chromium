// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/safety_checks.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/loader/content_security_notifier.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
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
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/isolation_info.h"
#include "net/storage_access_api/status.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/script_source_location.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#endif

namespace content {

DedicatedWorkerHost::DedicatedWorkerHost(
    DedicatedWorkerServiceImpl* service,
    const blink::DedicatedWorkerToken& token,
    RenderProcessHost* worker_process_host,
    DedicatedWorkerCreator creator,
    GlobalRenderFrameHostId ancestor_render_frame_host_id,
    const blink::StorageKey& creator_storage_key,
    const url::Origin& renderer_origin,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr creator_client_security_state,
    base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host,
    net::StorageAccessApiStatus storage_access_api_status)
    : service_(service),
      token_(token),
      worker_process_host_(worker_process_host),
      creator_(creator),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_storage_key.origin()),
      renderer_origin_(renderer_origin),
      // TODO(crbug.com/40051700): Calculate the worker origin based on
      // the worker script URL (the worker's storage key should have an opaque
      // origin if the worker script URL's scheme is data:).
      storage_key_(creator_storage_key),
      isolation_info_(isolation_info),
      reporting_source_(base::UnguessableToken::Create()),
      creator_client_security_state_(std::move(creator_client_security_state)),
      host_receiver_(this, std::move(host)),
      creator_coep_reporter_(std::move(creator_coep_reporter)),
      code_cache_host_receivers_(GetProcessHost()
                                     ->GetStoragePartition()
                                     ->GetGeneratedCodeCacheContext()),
      storage_access_api_status_(storage_access_api_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(worker_process_host_);
  DCHECK(worker_process_host_->IsInitializedAndNotDead());
  DCHECK(creator_client_security_state_);

  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  // TODO(https://crbug.com/11990077): Once we add more stuff to
  // `blink::StorageKey`, DCHECK that `storage_key` is consistent with
  // `isolation_info_` here (i.e. their origin and top frame origin match).

  scoped_process_host_observation_.Observe(worker_process_host_.get());

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
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  // If this instance is being destroyed because its mojo connection was
  // disconnected, then the destruction of the `service_worker_handle_` could
  // end up causing this instance to be deleted again through
  // RenderProcessExited, if that handle was the only thing keeping the
  // RenderProcessHost alive. Stop observing the RenderProcessHost right away to
  // avoid this issue. See https://crbug.com/383067308
  scoped_process_host_observation_.Reset();

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

  service_->NotifyBeforeWorkerDestroyed(token_, creator_);

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
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  delete this;
}

void DedicatedWorkerHost::RenderProcessExited(
    RenderProcessHost* render_process_host,
    const ChildProcessTerminationInfo& info) {
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  DCHECK_EQ(worker_process_host_, render_process_host);

  delete this;
}

void DedicatedWorkerHost::InProcessRendererExiting(
    RenderProcessHost* render_process_host) {
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  DCHECK_EQ(worker_process_host_, render_process_host);

  delete this;
}

void DedicatedWorkerHost::RenderProcessHostDestroyed(
    RenderProcessHost* render_process_host) {
  // This is never reached as either RenderProcessExited() or
  // InProcessRendererExiting() is guaranteed to be called before this function
  // and `this` is deleted there.
  NOTREACHED();
}

void DedicatedWorkerHost::StartScriptLoad(
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client,
    net::StorageAccessApiStatus storage_access_api_status) {
  script_request_url_ = script_url;
  TRACE_EVENT("loading", "DedicatedWorkerHost::StartScriptLoad", "script_url",
              script_url);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
    ScriptLoadStartFailed(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
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

  std::visit(absl::Overload(
                 [&](const GlobalRenderFrameHostId& render_frame_host_id) {
                   creator_render_frame_host =
                       RenderFrameHostImpl::FromID(render_frame_host_id);
                 },
                 [&](blink::DedicatedWorkerToken dedicated_worker_token) {
                   creator_worker = service_->GetDedicatedWorkerHostFromToken(
                       dedicated_worker_token);
                 }),
             creator_);

  if (!creator_render_frame_host && !creator_worker) {
    ScriptLoadStartFailed(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
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

  // For blob URL workers, inherit the controller from the worker's parent.
  // See https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
  // Also, we need the worker's parent to set FetchEvent::client_id.
  base::WeakPtr<ServiceWorkerClient> parent_service_worker_client;
  if (creator_render_frame_host) {
    // The creator of this worker is a frame.
    parent_service_worker_client =
        creator_render_frame_host->GetLastCommittedServiceWorkerClient();
  } else {
    parent_service_worker_client =
        creator_worker->service_worker_handle()->service_worker_client();
  }
  std::string fetch_event_client_id;
  if (parent_service_worker_client) {
    fetch_event_client_id = parent_service_worker_client->client_uuid();
  }

  service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
      storage_partition_impl->GetServiceWorkerContext(), base::DoNothing(),
      std::move(fetch_event_client_id),
      std::move(parent_service_worker_client));

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

  TRACE_EVENT_BEGIN("loading", "WorkerScriptFetcher CreateAndStart",
                    perfetto::Track::FromPointer(this));
  WorkerScriptFetcher::CreateAndStart(
      worker_process_host_->GetDeprecatedID(), token_, script_url,
      *nearest_ancestor_render_frame_host, creator_render_frame_host,
      nearest_ancestor_render_frame_host->ComputeSiteForCookies(),
      creator_origin_, storage_key_,
      nearest_ancestor_render_frame_host->GetIsolationInfoForSubresources(),
      std::move(client_security_state), credentials_mode,
      std::move(outside_fetch_client_settings_object),
      network::mojom::RequestDestination::kWorker,
      storage_partition_impl->GetServiceWorkerContext(),
      service_worker_handle_.get(), std::move(blob_url_loader_factory), nullptr,
      storage_partition_impl, partition_domain,
      DedicatedWorkerDevToolsAgentHost::GetFor(this), token_.value(),
      /*require_cross_site_request_for_cookies=*/false,
      storage_access_api_status,
      base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the dedicated worker scope");
}

void DedicatedWorkerHost::DidStartScriptLoad(
    std::optional<WorkerScriptFetcherResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // WorkerScriptFetcher CreateAndStart
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "DedicatedWorkerHost::DidStartScriptLoad",
              "final_response_url", script_request_url_);

  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  if (!result) {
    ScriptLoadStartFailed(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // TODO(crbug.com/41471904): Check if the main script's final response
  // URL is committable.
  final_response_url_ = result->final_response_url;
  service_->NotifyWorkerFinalResponseURLDetermined(token_,
                                                   result->final_response_url);

  // TODO(cammie): Change this approach when we support shared workers
  // creating dedicated workers, as there might be no ancestor frame.
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    ScriptLoadStartFailed(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // https://html.spec.whatwg.org/C/#run-a-worker
  if (result->final_response_url.SchemeIsLocal()) {
    // TODO(crbug.com/40053797): Inherit from the file creator instead
    // once creator policies are persisted through the filesystem store.
    worker_client_security_state_ = creator_client_security_state_->Clone();
  } else {
    CHECK(result->main_script_load_params);
    DCHECK(result->main_script_load_params->response_head);
    DCHECK(result->main_script_load_params->response_head->parsed_headers);

    worker_client_security_state_ = network::mojom::ClientSecurityState::New();
    worker_client_security_state_->ip_address_space = CalculateIPAddressSpace(
        result->final_response_url,
        result->main_script_load_params->response_head.get(),
        GetContentClient()->browser());
    worker_client_security_state_->is_web_secure_context =
        network::IsUrlPotentiallyTrustworthy(result->final_response_url) &&
        creator_client_security_state_->is_web_secure_context;
    // Deprecation trial status allowing LNA requests on non-http
    bool allow_non_secure_local_network_access =
        ancestor_render_frame_host->policy_container_host() &&
        ancestor_render_frame_host->policy_container_host()
            ->policies()
            .allow_non_secure_local_network_access;

    worker_client_security_state_->private_network_request_policy =
        DerivePrivateNetworkRequestPolicy(
            worker_client_security_state_->ip_address_space,
            worker_client_security_state_->is_web_secure_context,
            allow_non_secure_local_network_access,
            PrivateNetworkRequestContext::kWorker);

    // Check for policy overrides on LNA. For dedicated workers, we apply
    // policy based on origin of the document that owns the worker.
    // TODO(crbug.com/452389539): Centralize these policy overrides.
    ContentBrowserClient* client = GetContentClient()->browser();
    BrowserContext* context = ancestor_render_frame_host->GetBrowserContext();
    url::Origin origin = ancestor_render_frame_host->GetLastCommittedOrigin();
    ContentBrowserClient::PrivateNetworkRequestPolicyOverride policy_override =
        client->ShouldOverridePrivateNetworkRequestPolicy(context, origin);
    worker_client_security_state_->private_network_request_policy =
        OverrideLocalNetworkAccessPolicy(
            worker_client_security_state_->private_network_request_policy,
            policy_override);

    // > 14.6 Otherwise, set worker global scope's embedder policy to the result
    // of obtaining an embedder policy from response.
    worker_client_security_state_->cross_origin_embedder_policy =
        result->main_script_load_params->response_head->parsed_headers
            ->cross_origin_embedder_policy;

  }

  // The worker global scope's document isolation policy is the same as its
  // creator.
  worker_client_security_state_->document_isolation_policy =
      creator_client_security_state_->document_isolation_policy;

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  // Create a COEP reporter with worker's policy.
  const network::CrossOriginEmbedderPolicy& coep =
      worker_client_security_state_->cross_origin_embedder_policy;
  coep_reporter_ = std::make_unique<CrossOriginEmbedderPolicyReporter>(
      storage_partition->GetWeakPtr(), result->final_response_url,
      coep.reporting_endpoint, coep.report_only_reporting_endpoint,
      reporting_source_, isolation_info_.network_anonymization_key());
  mojo::PendingReceiver<blink::mojom::ReportingObserver>
      coep_reporting_observer;
  mojo::PendingRemote<blink::mojom::ReportingObserver> coep_reporting_remote;
  coep_reporting_observer =
      coep_reporting_remote.InitWithNewPipeAndPassReceiver();
  coep_reporter_->BindObserver(std::move(coep_reporting_remote));

  // Create a DIP reporter with worker's policy.
  const network::DocumentIsolationPolicy& dip =
      worker_client_security_state_->document_isolation_policy;
  dip_reporter_ = std::make_unique<DocumentIsolationPolicyReporter>(
      storage_partition->GetWeakPtr(), result->final_response_url,
      dip.reporting_endpoint, dip.report_only_reporting_endpoint,
      reporting_source_, isolation_info_.network_anonymization_key());
  mojo::PendingReceiver<blink::mojom::ReportingObserver> dip_reporting_observer;
  mojo::PendingRemote<blink::mojom::ReportingObserver> dip_reporting_remote;
  dip_reporting_observer =
      dip_reporting_remote.InitWithNewPipeAndPassReceiver();
  dip_reporter_->BindObserver(std::move(dip_reporting_remote));

  // > 14.8 If the result of checking a global object's embedder policy with
  // worker global scope, owner, and response is false, then set response to a
  // network error.
  if (!CheckCOEP()) {
    ScriptLoadStartFailed(network::URLLoaderCompletionStatus(
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
  result->subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  result->subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info;
  blink::mojom::ControllerServiceWorkerInfoPtr controller;
  if (service_worker_handle_->service_worker_client()) {
    // TODO(crbug.com/40153087): Propagate dedicated worker ukm::SourceId
    // here.
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter;
    if (GetWorkerCoepReporter()) {
      GetWorkerCoepReporter()->Clone(
          coep_reporter.InitWithNewPipeAndPassReceiver());
    }

    mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>
        dip_reporter;
    if (dip_reporter_) {
      dip_reporter_->Clone(dip_reporter.InitWithNewPipeAndPassReceiver());
    }
    std::tie(container_info, controller) =
        service_worker_handle_->scoped_service_worker_client()
            ->CommitResponseAndRelease(
                /*rfh_id=*/std::nullopt,
                std::move(result->policy_container_policies),
                std::move(coep_reporter), std::move(dip_reporter),
                ukm::kInvalidSourceId);
  }

  client_->OnScriptLoadStarted(
      std::move(container_info), std::move(result->main_script_load_params),
      std::move(result->subresource_loader_factories),
      subresource_loader_updater_.BindNewPipeAndPassReceiver(),
      std::move(controller),
      BindAndPassRemoteForBackForwardCacheControllerHost(),
      std::move(coep_reporting_observer), std::move(dip_reporting_observer));
  if (service_worker_handle_->service_worker_client()) {
    service_worker_handle_->service_worker_client()->SetContainerReady();
  }
}

void DedicatedWorkerHost::ScriptLoadStartFailed(
    const network::URLLoaderCompletionStatus& status) {
  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (ancestor_render_frame_host) {
    // Notify that the loading failed to DevTools. It fires
    // `Network.onLoadingFailed` event.
    devtools_instrumentation::OnWorkerMainScriptLoadingFailed(
        script_request_url_,
        DedicatedWorkerDevToolsAgentHost::GetFor(this)->devtools_worker_token(),
        FrameTreeNode::From(ancestor_render_frame_host),
        ancestor_render_frame_host, status);
  }

  client_->OnScriptLoadStartFailed();
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DedicatedWorkerHost::CreateNetworkFactoryForSubresources(
    RenderFrameHostImpl* ancestor_render_frame_host,
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ancestor_render_frame_host);
  DCHECK(bypass_redirect_checks);

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  if (GetWorkerCoepReporter()) {
    GetWorkerCoepReporter()->Clone(
        coep_reporter.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>
      dip_reporter;
  if (dip_reporter_) {
    dip_reporter_->Clone(dip_reporter.InitWithNewPipeAndPassReceiver());
  }

  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForFrame(
          ancestor_render_frame_host, GetStorageKey().origin(), isolation_info_,
          worker_client_security_state_->Clone(), std::move(coep_reporter),
          std::move(dip_reporter), worker_process_host_,
          ancestor_render_frame_host->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::
                  kPrivateStateTokenIssuance)
              ? network::mojom::TrustTokenOperationPolicyVerdict::
                    kPotentiallyPermit
              : network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
          ancestor_render_frame_host->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kTrustTokenRedemption)
              ? network::mojom::TrustTokenOperationPolicyVerdict::
                    kPotentiallyPermit
              : network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
          ancestor_render_frame_host->GetCookieSettingOverrides(),
          /*network_restrictions_id=*/std::nullopt,
          "DedicatedWorkerHost::CreateNetworkFactoryForSubresources");

  RenderFrameHost* frame = nullptr;
  if (base::FeatureList::IsEnabled(
          blink::features::kUseAncestorRenderFrameForWorker)) {
    frame = ancestor_render_frame_host;
  }

  return url_loader_factory::CreatePendingRemote(
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource,
      url_loader_factory::TerminalParams::ForNetworkContext(
          worker_process_host_->GetStoragePartition()->GetNetworkContext(),
          std::move(factory_params),
          url_loader_factory::HeaderClientOption::kAllow,
          url_loader_factory::FactoryOverrideOption::kAllow),
      url_loader_factory::ContentClientParams(
          worker_process_host_->GetBrowserContext(), frame,
          worker_process_host_->GetDeprecatedID(), GetStorageKey().origin(),
          isolation_info_,
          ukm::SourceIdObj::FromInt64(
              ancestor_render_frame_host->GetPageUkmSourceId()),
          bypass_redirect_checks),
      devtools_instrumentation::WillCreateURLLoaderFactoryParams::ForFrame(
          ancestor_render_frame_host));
}

// [spec]
// https://html.spec.whatwg.org/C/#check-a-global-object's-embedder-policy
bool DedicatedWorkerHost::CheckCOEP() {
  DCHECK(final_response_url_);

  if (!creator_coep_reporter_) {
    return false;
  }

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
          worker_cross_origin_embedder_policy.value)) {
    creator_coep_reporter_->QueueWorkerInitializationReport(
        final_response_url_.value(),
        /*report_only=*/true);
  }

  // [spec]: 5. If ownerPolicy's value is "unsafe-none" or policy's value is
  // "require-corp" or "credentialless", then return true.
  if (!network::CompatibleWithCrossOriginIsolated(
          creator_cross_origin_embedder_policy.value) ||
      network::CompatibleWithCrossOriginIsolated(
          worker_cross_origin_embedder_policy.value)) {
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
  if (!ancestor_render_frame_host) {
    return;
  }

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
          ancestor_render_frame_host->GetIsolationInfoForSubresources(),
          worker_client_security_state_->Clone()),
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
          worker_process_host_->GetDeprecatedID(),
          ancestor_render_frame_host->GetWeakPtr(), GetStorageKey().origin(),
          isolation_info_.network_anonymization_key(),
          worker_client_security_state_->Clone()),
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
  base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter =
      GetWorkerCoepReporter();

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_host_->GetDeprecatedID(), /*creator=*/token_,
          ancestor_render_frame_host_id_, GetStorageKey(), isolation_info_,
          worker_client_security_state_->Clone(), creator_coep_reporter),
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

bool DedicatedWorkerHost::WasStorageAccessGranted() {
  switch (storage_access_api_status_) {
    case net::StorageAccessApiStatus::kAccessViaAPI:
      return true;
    case net::StorageAccessApiStatus::kNone:
      return false;
  }
}

void DedicatedWorkerHost::CreateBlobUrlStoreProvider(
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      GetProcessHost()->GetStoragePartition());

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      GetStorageKey(), renderer_origin_, GetProcessHost()->GetDeprecatedID(),
      std::move(receiver),
      /*context_type_for_debugging=*/"Dedicated Worker",
      base::BindRepeating(
          [](base::WeakPtr<DedicatedWorkerHost> host) -> std::string {
            if (!host) {
              return "destroyed DedicatedWorkerHost";
            }
            return host->GetStorageKey().GetDebugString();
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<DedicatedWorkerHost> host) -> bool {
            if (!host) {
              return false;
            }
            return host->WasStorageAccessGranted();
          },
          weak_factory_.GetWeakPtr()),
      !(GetContentClient()->browser()->IsBlobUrlPartitioningEnabled(
          GetProcessHost()->GetBrowserContext())),
      storage::BlobURLValidityCheckBehavior::
          ALLOW_OPAQUE_ORIGIN_STORAGE_KEY_MISMATCH);
}

void DedicatedWorkerHost::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  RenderProcessHost* rph = GetProcessHost();
  code_cache_host_receivers_.Add(rph->GetDeprecatedID(),
                                 isolation_info_.network_isolation_key(),
                                 GetStorageKey(), std::move(receiver));
}

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

#if !BUILDFLAG(IS_ANDROID)
void DedicatedWorkerHost::BindHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  // The ancestor frame may have already been closed. In that case, the worker
  // will soon be terminated too, so abort the connection.
  if (!ancestor_render_frame_host) {
    return;
  }

  ancestor_render_frame_host->GetHidService(std::move(receiver));
}
#endif

void DedicatedWorkerHost::CreateBucketManagerHost(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  GetProcessHost()->BindBucketManagerHost(GetWeakPtr(), std::move(receiver));
}

void DedicatedWorkerHost::GetFileSystemAccessManager(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());
  auto* manager = storage_partition_impl->GetFileSystemAccessManager();
  manager->BindReceiver(
      FileSystemAccessManagerImpl::BindingContext(
          GetStorageKey(),
          // TODO(crbug.com/41473757): Obtain and use a better
          // URL for workers instead of the origin as source url.
          // This URL will be used for SafeBrowsing checks and for
          // the Quarantine Service.
          GetStorageKey().origin().GetURL(), GetAncestorRenderFrameHostId(),
          /*is_worker=*/true),
      std::move(receiver));
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
void DedicatedWorkerHost::BindPressureService(
    mojo::PendingReceiver<blink::mojom::WebPressureManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!network::IsOriginPotentiallyTrustworthy(creator_origin_)) {
    return;
  }

  // https://www.w3.org/TR/compute-pressure/#policy-control
  auto* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    return;
  }

  if (!ancestor_render_frame_host->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kComputePressure)) {
    ancestor_render_frame_host->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "This frame is connected to a Dedicated Worker that has requested "
        "access to the Compute Pressure API. This worker can't access the API "
        "because this frame is not allowed to access this feature due to "
        "Permissions Policy.");
    return;
  }

  if (!pressure_service_) {
    pressure_service_ =
        std::make_unique<PressureServiceForDedicatedWorker>(this);
  }

  pressure_service_->BindReceiver(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

void DedicatedWorkerHost::ObserveNetworkServiceCrash(
    StoragePartitionImpl* storage_partition_impl) {
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = worker_process_host_->GetDeprecatedID();
  params->debug_tag = "DedicatedWorkerHost::ObserveNetworkServiceCrash";
  network_service_connection_error_handler_holder_.reset();
  storage_partition_impl->GetNetworkContext()->CreateURLLoaderFactory(
      network_service_connection_error_handler_holder_
          .BindNewPipeAndPassReceiver(),
      std::move(params));
  network_service_connection_error_handler_holder_.set_disconnect_handler(
      base::BindOnce(&DedicatedWorkerHost::OnNetworkServiceCrash,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::OnNetworkServiceCrash() {
  DCHECK(IsOutOfProcessNetworkService());
  DCHECK(subresource_loader_updater_.is_bound());
  DCHECK(network_service_connection_error_handler_holder_);
  DCHECK(!network_service_connection_error_handler_holder_.is_connected());

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());
  // Start observing Network Service crash again.
  ObserveNetworkServiceCrash(storage_partition_impl);

  UpdateSubresourceLoaderFactories();
}

void DedicatedWorkerHost::UpdateSubresourceLoaderFactories() {
  // Ignore DevTools attempts to update loader factories before
  // the main script started loading.
  if (!subresource_loader_updater_.is_bound()) {
    return;
  }

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    return;
  }

  // Get a storage domain.
  auto partition_domain =
      ancestor_render_frame_host->GetSiteInstance()->GetPartitionDomain(
          storage_partition_impl);

  // If this is a nested worker, there is no creator frame and
  // |creator_render_frame_host| will be null.
  const content::GlobalRenderFrameHostId* const render_frame_host_id =
      std::get_if<content::GlobalRenderFrameHostId>(&creator_);
  RenderFrameHostImpl* creator_render_frame_host =
      render_frame_host_id ? RenderFrameHostImpl::FromID(*render_frame_host_id)
                           : nullptr;

  // Recreate the default URLLoaderFactory.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories = WorkerScriptFetcher::CreateFactoryBundle(
          WorkerScriptFetcher::LoaderType::kSubResource,
          worker_process_host_->GetDeprecatedID(), storage_partition_impl,
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

base::WeakPtr<CrossOriginEmbedderPolicyReporter>
DedicatedWorkerHost::GetWorkerCoepReporter() {
  DCHECK(coep_reporter_);
  return coep_reporter_->GetWeakPtr();
}

void DedicatedWorkerHost::EvictFromBackForwardCache(
    blink::mojom::RendererEvictionReason reason,
    blink::mojom::ScriptSourceLocationPtr source) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The frame may have already been closed.
    return;
  }
  ancestor_render_frame_host->EvictFromBackForwardCache(std::move(reason),
                                                        std::move(source));
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
      ->GetPermissionStatusForWorker(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(permission_type),
          GetProcessHost(), GetStorageKey().origin());
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
  mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>
      dip_reporter;
  if (dip_reporter_) {
    dip_reporter_->Clone(dip_reporter.InitWithNewPipeAndPassReceiver());
  }
  worker_process_host_->BindCacheStorage(
      cross_origin_embedder_policy(), std::move(coep_reporter),
      worker_client_security_state_->document_isolation_policy,
      std::move(dip_reporter), bucket_locator, std::move(receiver));
}

void DedicatedWorkerHost::GetSandboxedFileSystemForBucket(
    const storage::BucketInfo& bucket,
    const std::vector<std::string>& directory_path_components,
    blink::mojom::BucketHost::GetDirectoryCallback callback) {
  GetProcessHost()->GetSandboxedFileSystemForBucket(
      bucket.ToBucketLocator(), directory_path_components, std::move(callback));
}

storage::BucketClientInfo DedicatedWorkerHost::GetBucketClientInfo() const {
  const auto* ancestor_rfh =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  return storage::BucketClientInfo{
      worker_process_host_->GetDeprecatedID(), GetToken(),
      ancestor_rfh ? std::optional(ancestor_rfh->GetDocumentToken())
                   : std::nullopt};
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

base::WeakPtr<ServiceWorkerClient>
DedicatedWorkerHost::GetServiceWorkerClient() {
  if (!service_worker_handle_) {
    return nullptr;
  }
  return service_worker_handle_->service_worker_client();
}

mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
DedicatedWorkerHost::BindAndPassRemoteForBackForwardCacheControllerHost() {
  return back_forward_cache_controller_host_receiver_
      .BindNewPipeAndPassRemote();
}

}  // namespace content
