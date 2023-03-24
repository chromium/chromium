// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/types/optional_util.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

bool SchemeMaySupportRedirectingToHTTPS(BrowserContext* browser_context,
                                        const GURL& url) {
  // If there is a registered protocol handler for this scheme, the embedder is
  // expected to redirect `url` to a registered URL in a URLLoaderThrottle, and
  // the interceptor will operate on the registered URL. Note that the HTML
  // specification requires that the registered URL is HTTPS.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  if (GetContentClient()->browser()->HasCustomSchemeHandler(browser_context,
                                                            url.scheme()))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return url.SchemeIs(kExternalFileScheme);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Returns true if a ServiceWorkerMainResourceLoaderInterceptor should be
// created for a worker with this |url|.
bool ShouldCreateForWorker(
    const GURL& url,
    base::WeakPtr<ServiceWorkerContainerHost> parent_container_host) {
  // Blob URL can be controlled by a parent's controller.
  if (url.SchemeIsBlob() && parent_container_host)
    return true;
  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url);
}

}  // namespace

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerMainResourceLoaderInterceptor::CreateForNavigation(
    const GURL& url,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle,
    const NavigationRequestInfo& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateForNavigation(
          url, request_info.common_params->request_destination,
          navigation_handle->context_wrapper()->browser_context())) {
    return nullptr;
  }

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle),
      request_info.common_params->request_destination,
      request_info.begin_params->skip_service_worker,
      request_info.are_ancestors_secure, request_info.frame_tree_node_id,
      ChildProcessHost::kInvalidUniqueID, /* worker_token = */ nullptr,
      request_info.isolation_info));
}

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerMainResourceLoaderInterceptor::CreateForWorker(
    const network::ResourceRequest& resource_request,
    const net::IsolationInfo& isolation_info,
    int process_id,
    const DedicatedOrSharedWorkerToken& worker_token,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(resource_request.destination ==
             network::mojom::RequestDestination::kWorker ||
         resource_request.destination ==
             network::mojom::RequestDestination::kSharedWorker)
      << resource_request.destination;

  if (!ShouldCreateForWorker(resource_request.url,
                             navigation_handle->parent_container_host()))
    return nullptr;

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle), resource_request.destination,
      resource_request.skip_service_worker, /*are_ancestors_secure=*/false,
      FrameTreeNode::kFrameTreeNodeInvalidId, process_id, &worker_token,
      isolation_info));
}

ServiceWorkerMainResourceLoaderInterceptor::
    ~ServiceWorkerMainResourceLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerMainResourceLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);

  ServiceWorkerContextCore* context_core =
      handle_->context_wrapper()->context();
  if (!context_core || !browser_context) {
    std::move(loader_callback).Run(/*handler=*/{});
    return;
  }

  // If this is the first request before redirects, a container info and
  // container host has not yet been created.
  if (!handle_->has_container_info()) {
    // Create `container_info`.
    auto container_info =
        blink::mojom::ServiceWorkerContainerInfoForClient::New();
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver =
            container_info->host_remote.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote;

    container_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    handle_->OnCreatedContainerHost(std::move(container_info));

    // Create the ServiceWorkerContainerHost. Its lifetime is bound to
    // `container_info`.
    DCHECK(!handle_->container_host());
    base::WeakPtr<ServiceWorkerContainerHost> container_host;
    bool inherit_controller_only = false;

    if (blink::IsRequestDestinationFrame(request_destination_)) {
      container_host = context_core->CreateContainerHostForWindow(
          std::move(host_receiver), are_ancestors_secure_,
          std::move(client_remote), frame_tree_node_id_);
    } else {
      DCHECK(request_destination_ ==
                 network::mojom::RequestDestination::kWorker ||
             request_destination_ ==
                 network::mojom::RequestDestination::kSharedWorker);

      ServiceWorkerClientInfo client_info =
          ServiceWorkerClientInfo(*worker_token_);

      container_host = context_core->CreateContainerHostForWorker(
          std::move(host_receiver), process_id_, std::move(client_remote),
          client_info);

      // For the blob worker case, inherit the controller from the worker's
      // parent. See
      // https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
      base::WeakPtr<ServiceWorkerContainerHost> parent_container_host =
          handle_->parent_container_host();
      if (parent_container_host &&
          tentative_resource_request.url.SchemeIsBlob()) {
        container_host->InheritControllerFrom(*parent_container_host,
                                              tentative_resource_request.url);
        inherit_controller_only = true;
      }
    }
    DCHECK(container_host);
    handle_->set_container_host(container_host);

    // For the blob worker case, we only inherit the controller and do not let
    // it intercept the main resource request. Blob URLs are not eligible to
    // go through service worker interception. So just call the loader
    // callback now.
    if (inherit_controller_only) {
      std::move(loader_callback).Run(/*handler=*/{});
      return;
    }
  }

  // Update `isolation_info_`  to equal the net::IsolationInfo needed for any
  // service worker intercepting this request. Here, `isolation_info_` directly
  // corresponds to the StorageKey used to look up the service worker's
  // registration. That StorageKey will then be used later to recreate this
  // net::IsolationInfo for use by the ServiceWorker itself.
  url::Origin new_origin = url::Origin::Create(tentative_resource_request.url);
  net::SiteForCookies new_site_for_cookies = isolation_info_.site_for_cookies();
  new_site_for_cookies.CompareWithFrameTreeOriginAndRevise(new_origin);
  isolation_info_ = net::IsolationInfo::Create(
      isolation_info_.request_type(),
      isolation_info_.top_frame_origin().value(), new_origin,
      new_site_for_cookies, absl::nullopt, isolation_info_.nonce());

  // Attempt to get the storage key from |RenderFrameHostImpl|. This correctly
  // accounts for extension URLs. The absence of this logic was a potential
  // cause for https://crbug.com/1346450.
  absl::optional<blink::StorageKey> storage_key =
      GetStorageKeyFromRenderFrameHost(
          new_origin, base::OptionalToPtr(isolation_info_.nonce()));
  if (!storage_key.has_value()) {
    storage_key = GetStorageKeyFromWorkerHost(new_origin);
  }
  if (!storage_key.has_value()) {
    // If we're in this case then we couldn't get the StorageKey from the RFH,
    // which means we also can't get the storage partitioning status from
    // RuntimeFeatureState(Read)Context. Using
    // CreateFromOriginAndIsolationInfo() will create a key based on
    // net::features::kThirdPartyStoragePartitioning state.
    storage_key = blink::StorageKey::CreateFromOriginAndIsolationInfo(
        new_origin, isolation_info_);
  }

  // If we know there's no service worker for the storage key, let's skip asking
  // the storage to check the existence.
  bool skip_service_worker =
      skip_service_worker_ ||
      !OriginCanAccessServiceWorkers(tentative_resource_request.url) ||
      !handle_->context_wrapper()->MaybeHasRegistrationForStorageKey(
          *storage_key);

  // Create and start the handler for this request. It will invoke the loader
  // callback or fallback callback.
  request_handler_ = std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context_core->AsWeakPtr(), handle_->container_host(),
      request_destination_, skip_service_worker, frame_tree_node_id_,
      handle_->service_worker_accessed_callback());

  request_handler_->MaybeCreateLoader(
      tentative_resource_request, *storage_key, browser_context,
      std::move(loader_callback), std::move(fallback_callback));
}

absl::optional<SubresourceLoaderParams>
ServiceWorkerMainResourceLoaderInterceptor::
    MaybeCreateSubresourceLoaderParams() {
  if (!handle_) {
    return absl::nullopt;
  }
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      handle_->container_host();

  // We didn't find a matching service worker for this request, and
  // ServiceWorkerContainerHost::SetControllerRegistration() was not called.
  if (!container_host || !container_host->controller()) {
    return absl::nullopt;
  }

  // Otherwise let's send the controller service worker information along
  // with the navigation commit.
  SubresourceLoaderParams params;
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->mode = container_host->GetControllerMode();
  controller_info->fetch_handler_type =
      container_host->controller()->fetch_handler_type();
  controller_info->effective_fetch_handler_type =
      container_host->controller()->EffectiveFetchHandlerType();
  controller_info->fetch_handler_bypass_option =
      container_host->controller()->fetch_handler_bypass_option();
  controller_info->sha256_script_checksum =
      container_host->controller()->sha256_script_checksum();
  // Note that |controller_info->remote_controller| is null if the controller
  // has no fetch event handler. In that case the renderer frame won't get the
  // controller pointer upon the navigation commit, and subresource loading will
  // not be intercepted. (It might get intercepted later if the controller
  // changes due to skipWaiting() so SetController is sent.)
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote =
      container_host->GetRemoteControllerServiceWorker();
  if (remote.is_bound()) {
    controller_info->remote_controller = remote.Unbind();
  }

  controller_info->client_id = container_host->client_uuid();
  if (container_host->fetch_request_window_id()) {
    controller_info->fetch_request_window_id =
        absl::make_optional(container_host->fetch_request_window_id());
  }
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      container_host->GetOrCreateServiceWorkerObjectHost(
          container_host->controller());
  if (object_host) {
    params.controller_service_worker_object_host = object_host;
    controller_info->object_info = object_host->CreateIncompleteObjectInfo();
  }
  for (const auto feature : container_host->controller()->used_features()) {
    controller_info->used_features.push_back(feature);
  }
  params.controller_service_worker_info = std::move(controller_info);
  return absl::optional<SubresourceLoaderParams>(std::move(params));
}

ServiceWorkerMainResourceLoaderInterceptor::
    ServiceWorkerMainResourceLoaderInterceptor(
        base::WeakPtr<ServiceWorkerMainResourceHandle> handle,
        network::mojom::RequestDestination request_destination,
        bool skip_service_worker,
        bool are_ancestors_secure,
        int frame_tree_node_id,
        int process_id,
        const DedicatedOrSharedWorkerToken* worker_token,
        const net::IsolationInfo& isolation_info)
    : handle_(std::move(handle)),
      request_destination_(request_destination),
      skip_service_worker_(skip_service_worker),
      isolation_info_(isolation_info),
      are_ancestors_secure_(are_ancestors_secure),
      frame_tree_node_id_(frame_tree_node_id),
      process_id_(process_id),
      worker_token_(base::OptionalFromPtr(worker_token)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
}

// static
bool ServiceWorkerMainResourceLoaderInterceptor::ShouldCreateForNavigation(
    const GURL& url,
    network::mojom::RequestDestination request_destination,
    BrowserContext* browser_context) {
  // <embed> and <object> navigations must bypass the service worker, per the
  // discussion in https://w3c.github.io/ServiceWorker/#implementer-concerns.
  if (request_destination == network::mojom::RequestDestination::kEmbed ||
      request_destination == network::mojom::RequestDestination::kObject) {
    return false;
  }

  // Create the interceptor even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url) ||
         SchemeMaySupportRedirectingToHTTPS(browser_context, url);
}

absl::optional<blink::StorageKey>
ServiceWorkerMainResourceLoaderInterceptor::GetStorageKeyFromRenderFrameHost(
    const url::Origin& origin,
    const base::UnguessableToken* nonce) {
  // In this case |frame_tree_node_id_| is invalid.
  if (!blink::IsRequestDestinationFrame(request_destination_))
    return absl::nullopt;
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node)
    return absl::nullopt;
  RenderFrameHostImpl* frame_host = frame_tree_node->current_frame_host();
  if (!frame_host)
    return absl::nullopt;

  return frame_host->CalculateStorageKey(origin, nonce);
}

absl::optional<blink::StorageKey>
ServiceWorkerMainResourceLoaderInterceptor::GetStorageKeyFromWorkerHost(
    const url::Origin& origin) {
  if (!worker_token_.has_value())
    return absl::nullopt;
  auto* process = RenderProcessHost::FromID(process_id_);
  if (!process)
    return absl::nullopt;
  auto* storage_partition = process->GetStoragePartition();

  if (worker_token_->Is<blink::DedicatedWorkerToken>()) {
    auto* worker_service = static_cast<DedicatedWorkerServiceImpl*>(
        storage_partition->GetDedicatedWorkerService());
    auto* worker_host = worker_service->GetDedicatedWorkerHostFromToken(
        worker_token_->GetAs<blink::DedicatedWorkerToken>());
    if (worker_host)
      return worker_host->GetStorageKey().WithOrigin(origin);
  } else if (worker_token_->Is<blink::SharedWorkerToken>()) {
    auto* worker_service = static_cast<SharedWorkerServiceImpl*>(
        storage_partition->GetSharedWorkerService());
    auto* worker_host = worker_service->GetSharedWorkerHostFromToken(
        worker_token_->GetAs<blink::SharedWorkerToken>());
    if (worker_host)
      return worker_host->GetStorageKey().WithOrigin(origin);
  } else {
    NOTREACHED();
  }
  return absl::nullopt;
}

}  // namespace content
