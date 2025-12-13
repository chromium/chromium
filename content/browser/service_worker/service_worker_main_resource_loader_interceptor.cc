// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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
                                                            url.GetScheme())) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  return url.SchemeIs(kExternalFileScheme);
#else   // BUILDFLAG(IS_CHROMEOS)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
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
                                              request_info.frame_tree_node_id),
      request_info.isolation_info);

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle)));
}

std::unique_ptr<ServiceWorkerMainResourceLoaderInterceptor>
ServiceWorkerMainResourceLoaderInterceptor::CreateForPrefetch(
    const network::ResourceRequest& resource_request,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle,
    scoped_refptr<network::SharedURLLoaderFactory> network_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));

  if (!ShouldCreateForNavigation(
          resource_request.url, resource_request.destination,
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
          .CreateServiceWorkerClientForPrefetch(
              std::move(network_url_loader_factory)),
      resource_request.trusted_params->isolation_info);

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle)));
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
              absl::ConvertVariantTo<ServiceWorkerClientInfo>(worker_token)),
      isolation_info);

  // TODO(crbug.com/324939068): remove this UMA after the launch.
  if (resource_request.destination ==
      network::mojom::RequestDestination::kSharedWorker) {
    base::UmaHistogramBoolean("ServiceWorker.SharedWorkerScript.IsBlob",
                              resource_request.url.SchemeIsBlob());
    if (resource_request.url.SchemeIsBlob() &&
        navigation_handle->service_worker_client() &&
        navigation_handle->service_worker_client()->controller()) {
      navigation_handle->service_worker_client()->controller()->CountFeature(
          blink::mojom::WebFeature::
              kSharedWorkerScriptUnderServiceWorkerControlIsBlob);
    }
  }

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle)));
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

  CHECK(handle_->InitializeForRequest(
      tentative_resource_request.url,
      ServiceWorkerMainResourceHandle::TopFrameOriginForInitializeForRequest(
          tentative_resource_request),
      /*client_for_prefetch=*/nullptr));

  // If we know there's no service worker for the storage key, let's skip asking
  // the storage to check the existence.
  //
  // crbug.com/352578800: `MaybeHasRegistrationForStorageKey()` doesn't reflect
  // the fake registration initially. If the URL is eligible for
  // SyntheticResponse, do not skip service worker.
  bool skip_service_worker =
      tentative_resource_request.skip_service_worker ||
      !OriginCanAccessServiceWorkers(tentative_resource_request.url) ||
      !(handle_->context_wrapper()->MaybeHasRegistrationForStorageKey(
            handle_->service_worker_client()->key()) ||
        service_worker_loader_helpers::IsEligibleForSyntheticResponse(
            handle_->context_wrapper()->browser_context(),
            tentative_resource_request.url));

  // Create and start the handler for this request. It will invoke the loader
  // callback or fallback callback.
  request_handler_ = std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context_core->AsWeakPtr(), handle_->fetch_event_client_id(),
      handle_->service_worker_client(), skip_service_worker,
      handle_->service_worker_accessed_callback());

  request_handler_->MaybeCreateLoader(
      tentative_resource_request, browser_context, std::move(loader_callback),
      std::move(fallback_callback));
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
        base::WeakPtr<ServiceWorkerMainResourceHandle> handle)
    : handle_(std::move(handle)) {
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
