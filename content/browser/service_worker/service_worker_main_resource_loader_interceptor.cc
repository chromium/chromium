// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/isolation_info.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
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
    base::WeakPtr<ServiceWorkerClient> parent_service_worker_client) {
  // Blob URL can be controlled by a parent's controller.
  if (url.SchemeIsBlob() && parent_service_worker_client) {
    return true;
  }
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

  if (!navigation_handle->context_wrapper()->context()) {
    return nullptr;
  }

  navigation_handle->set_service_worker_client(
      navigation_handle->context_wrapper()
          ->context()
          ->service_worker_client_owner()
          .CreateServiceWorkerClientForWindow(request_info.are_ancestors_secure,
                                              request_info.frame_tree_node_id));

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle),
      request_info.begin_params->skip_service_worker,
      request_info.frame_tree_node_id, request_info.isolation_info));
}

std::unique_ptr<ServiceWorkerMainResourceLoaderInterceptor>
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

  if (!ShouldCreateForWorker(
          resource_request.url,
          navigation_handle->parent_service_worker_client())) {
    return nullptr;
  }

  if (!navigation_handle->context_wrapper()->context()) {
    return nullptr;
  }

  navigation_handle->set_service_worker_client(
      navigation_handle->context_wrapper()
          ->context()
          ->service_worker_client_owner()
          .CreateServiceWorkerClientForWorker(
              process_id,
              absl::ConvertVariantTo<ServiceWorkerClientInfo>(worker_token)));

  // TODO(crbug.com/324939068): remove this UMA after the launch.
  if (resource_request.destination ==
      network::mojom::RequestDestination::kSharedWorker) {
    base::UmaHistogramBoolean("ServiceWorker.SharedWorkerScript.IsBlob",
                              resource_request.url.SchemeIsBlob());
  }

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle), resource_request.skip_service_worker,
      FrameTreeNodeId(), isolation_info));
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
    CompleteWithoutLoader(std::move(loader_callback),
                          handle_->service_worker_client());
    return;
  }

  if ((tentative_resource_request.destination ==
           network::mojom::RequestDestination::kSharedWorker &&
       base::FeatureList::IsEnabled(kSharedWorkerBlobURLFix) &&
       GetContentClient()->browser()->AllowSharedWorkerBlobURLFix(
           browser_context)) ||
      tentative_resource_request.destination ==
          network::mojom::RequestDestination::kWorker) {
    // For the blob worker case, inherit the controller from the worker's
    // parent. See
    // https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
    base::WeakPtr<ServiceWorkerClient> parent_service_worker_client =
        handle_->parent_service_worker_client();
    if (parent_service_worker_client &&
        tentative_resource_request.url.SchemeIsBlob()) {
      handle_->service_worker_client()->InheritControllerFrom(
          *parent_service_worker_client,
          net::SimplifyUrlForRequest(tentative_resource_request.url));
      // For the blob worker case, we only inherit the controller and do not
      // let it intercept the main resource request. Blob URLs are not
      // eligible to go through service worker interception. So just call the
      // loader callback now.
      CompleteWithoutLoader(std::move(loader_callback),
                            handle_->service_worker_client());
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
      new_site_for_cookies, isolation_info_.nonce());

  // TODO(crbug.com/368025734): Move this `CalculateStorageKeyForUpdateUrls()`
  // to the subsequent call site of `UpdateUrls()`.
  blink::StorageKey storage_key =
      handle_->service_worker_client()->CalculateStorageKeyForUpdateUrls(
          tentative_resource_request.url, isolation_info_);

  // If we know there's no service worker for the storage key, let's skip asking
  // the storage to check the existence.
  bool skip_service_worker =
      skip_service_worker_ ||
      !OriginCanAccessServiceWorkers(tentative_resource_request.url) ||
      !handle_->context_wrapper()->MaybeHasRegistrationForStorageKey(
          storage_key);

  // Create and start the handler for this request. It will invoke the loader
  // callback or fallback callback.
  request_handler_ = std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context_core->AsWeakPtr(), handle_->fetch_event_client_id(),
      handle_->service_worker_client(), skip_service_worker,
      frame_tree_node_id_, handle_->service_worker_accessed_callback());

  request_handler_->MaybeCreateLoader(
      tentative_resource_request, storage_key, browser_context,
      std::move(loader_callback), std::move(fallback_callback));
}

void ServiceWorkerMainResourceLoaderInterceptor::CompleteWithoutLoader(
    LoaderCallback loader_callback,
    base::WeakPtr<ServiceWorkerClient> service_worker_client) {
  if (service_worker_client && service_worker_client->controller()) {
    std::move(loader_callback)
        .Run(NavigationLoaderInterceptor::Result(
            /*factory=*/nullptr, SubresourceLoaderParams()));
    return;
  }

  std::move(loader_callback).Run(std::nullopt);
}

ServiceWorkerMainResourceLoaderInterceptor::
    ServiceWorkerMainResourceLoaderInterceptor(
        base::WeakPtr<ServiceWorkerMainResourceHandle> handle,
        bool skip_service_worker,
        FrameTreeNodeId frame_tree_node_id,
        const net::IsolationInfo& isolation_info)
    : handle_(std::move(handle)),
      skip_service_worker_(skip_service_worker),
      isolation_info_(isolation_info),
      frame_tree_node_id_(frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
  CHECK(handle_->scoped_service_worker_client());
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

}  // namespace content
