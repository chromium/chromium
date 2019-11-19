// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace content {

namespace {

///////////////////////////////////////////////////////////////////////////////
// Core thread helpers

void LoaderCallbackWrapperOnCoreThread(
    ServiceWorkerNavigationHandleCore* handle_core,
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  base::Optional<SubresourceLoaderParams> subresource_loader_params;
  if (handle_core->interceptor()) {
    subresource_loader_params =
        handle_core->interceptor()->MaybeCreateSubresourceLoaderParams();
  }

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper,
          interceptor_on_ui, std::move(subresource_loader_params),
          std::move(loader_callback), std::move(handler)));
}

void FallbackCallbackWrapperOnCoreThread(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper,
          interceptor_on_ui, std::move(fallback_callback),
          reset_subresource_loader_params));
}

void InvokeRequestHandlerOnCoreThread(
    SingleRequestURLLoaderFactory::RequestHandler handler,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::move(handler).Run(resource_request, std::move(receiver),
                         std::move(client_remote));
}

// Does setup on the the core thread and calls back to
// |interceptor_on_ui->LoaderCallbackWrapper()| on the UI thread.
void MaybeCreateLoaderOnCoreThread(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    ServiceWorkerNavigationHandleCore* handle_core,
    const ServiceWorkerNavigationLoaderInterceptorParams& params,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote,
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool initialize_provider_only) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  ServiceWorkerContextCore* context_core =
      handle_core->context_wrapper()->context();
  ResourceContext* resource_context =
      ServiceWorkerContext::IsServiceWorkerOnUIEnabled()
          ? nullptr
          : handle_core->context_wrapper()->resource_context();
  if (!context_core || (!resource_context && !browser_context)) {
    LoaderCallbackWrapperOnCoreThread(handle_core, std::move(interceptor_on_ui),
                                      std::move(loader_callback),
                                      /*handler=*/{});
    return;
  }

  if (!handle_core->provider_host()) {
    // This is the initial request before redirects, so make the provider host.
    // Its lifetime is tied to the |provider_info| in the
    // ServiceWorkerNavigationHandle on the UI thread and which will be passed
    // to the renderer when the navigation commits.
    DCHECK(host_receiver);
    DCHECK(client_remote);
    base::WeakPtr<ServiceWorkerProviderHost> provider_host;

    if (params.resource_type == ResourceType::kMainFrame ||
        params.resource_type == ResourceType::kSubFrame) {
      provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
          context_core->AsWeakPtr(), params.are_ancestors_secure,
          params.frame_tree_node_id, std::move(host_receiver),
          std::move(client_remote));
    } else {
      DCHECK(params.resource_type == ResourceType::kWorker ||
             params.resource_type == ResourceType::kSharedWorker);
      auto provider_type =
          params.resource_type == ResourceType::kWorker
              ? blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker
              : blink::mojom::ServiceWorkerProviderType::kForSharedWorker;
      provider_host = ServiceWorkerProviderHost::CreateForWebWorker(
          context_core->AsWeakPtr(), params.process_id, provider_type,
          std::move(host_receiver), std::move(client_remote));
    }
    DCHECK(provider_host);
    handle_core->set_provider_host(provider_host);

    // Also make the inner interceptor.
    DCHECK(!handle_core->interceptor());
    handle_core->set_interceptor(
        std::make_unique<ServiceWorkerControlleeRequestHandler>(
            context_core->AsWeakPtr(), provider_host, params.resource_type,
            params.skip_service_worker));
  }

  // If |initialize_provider_only| is true, we have already determined there is
  // no registered service worker on the UI thread, so just initialize the
  // provider for this request.
  if (initialize_provider_only) {
    handle_core->interceptor()->InitializeProvider(tentative_resource_request);
    LoaderCallbackWrapperOnCoreThread(handle_core, interceptor_on_ui,
                                      std::move(loader_callback),
                                      /*handler=*/{});
    return;
  }

  // Start the inner interceptor. We continue in
  // LoaderCallbackWrapperOnCoreThread().
  //
  // It's safe to bind the raw |handle_core| to the callback because it owns the
  // interceptor, which invokes the callback.
  handle_core->interceptor()->MaybeCreateLoader(
      tentative_resource_request, browser_context, resource_context,
      base::BindOnce(&LoaderCallbackWrapperOnCoreThread, handle_core,
                     interceptor_on_ui, std::move(loader_callback)),
      base::BindOnce(&FallbackCallbackWrapperOnCoreThread, interceptor_on_ui,
                     std::move(fallback_callback)));
}
///////////////////////////////////////////////////////////////////////////////

}  // namespace

ServiceWorkerNavigationLoaderInterceptor::
    ServiceWorkerNavigationLoaderInterceptor(
        const ServiceWorkerNavigationLoaderInterceptorParams& params,
        base::WeakPtr<ServiceWorkerNavigationHandle> handle)
    : handle_(std::move(handle)), params_(params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
}

ServiceWorkerNavigationLoaderInterceptor::
    ~ServiceWorkerNavigationLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerNavigationLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);

  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;

  // If this is the first request before redirects, a provider info has not yet
  // been created.
  if (!handle_->has_provider_info()) {
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    host_receiver =
        provider_info->host_remote.InitWithNewEndpointAndPassReceiver();
    provider_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    handle_->OnCreatedProviderHost(std::move(provider_info));
  }

  bool initialize_provider_only = false;
  LoaderCallback original_callback;
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled() &&
      !handle_->context_wrapper()->HasRegistrationForOrigin(
          tentative_resource_request.url.GetOrigin())) {
    // We have no registrations, so it's safe to continue the request now
    // without blocking on the IO thread. Give a dummy callback to the
    // IO thread interceptor, and we'll run the original callback immediately
    // after starting it.
    original_callback = std::move(loader_callback);
    loader_callback =
        base::BindOnce([](scoped_refptr<network::SharedURLLoaderFactory>) {});
    initialize_provider_only = true;
  }

  // Start the inner interceptor on the core thread. It will call back to
  // LoaderCallbackWrapper() on the UI thread.
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&MaybeCreateLoaderOnCoreThread, GetWeakPtr(),
                     handle_->core(), params_, std::move(host_receiver),
                     std::move(client_remote), tentative_resource_request,
                     browser_context, std::move(loader_callback),
                     std::move(fallback_callback), initialize_provider_only));

  if (original_callback)
    std::move(original_callback).Run({});
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerNavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(subresource_loader_params_);
}

void ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper(
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // For worker main script requests, |handle_| can be destroyed during
  // interception. The initiator of this interceptor (i.e., WorkerScriptLoader)
  // will handle the case.
  // For navigation requests, this case should not happen because it's
  // guaranteed that this interceptor is destroyed before |handle_|.
  if (!handle_) {
    std::move(loader_callback).Run({});
    return;
  }

  subresource_loader_params_ = std::move(subresource_loader_params);

  if (!handler_on_core_thread) {
    std::move(loader_callback).Run({});
    return;
  }

  // The inner core thread interceptor wants to handle the request. However,
  // |handler_on_core_thread| expects to run on the core thread. Give our own
  // wrapper to the loader callback.
  std::move(loader_callback)
      .Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper,
          GetWeakPtr(), std::move(handler_on_core_thread))));
}

void ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper(
    FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(fallback_callback).Run(reset_subresource_loader_params);
}

base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor>
ServiceWorkerNavigationLoaderInterceptor::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper(
    SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(InvokeRequestHandlerOnCoreThread,
                     std::move(handler_on_core_thread), resource_request,
                     std::move(receiver), std::move(client)));
}

}  // namespace content
