// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestInterceptor;
}

namespace network {
class ResourceRequestBody;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandleCore;
class ServiceWorkerProviderHost;
class WebContents;

// Abstract base class for routing network requests to ServiceWorkers.
// Created one per URLRequest and attached to each request.
class CONTENT_EXPORT ServiceWorkerRequestHandler
    : public base::SupportsUserData::Data,
      public NavigationLoaderInterceptor {
 public:
  // PlzNavigate
  // Attaches a newly created handler if the given |request| needs to be handled
  // by ServiceWorker.
  static void InitializeForNavigation(
      net::URLRequest* request,
      ServiceWorkerNavigationHandleCore* navigation_handle_core,
      storage::BlobStorageContext* blob_storage_context,
      bool skip_service_worker,
      ResourceType resource_type,
      blink::mojom::RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      bool is_parent_frame_secure,
      scoped_refptr<network::ResourceRequestBody> body,
      base::RepeatingCallback<WebContents*()> web_contents_getter);

  // S13nServiceWorker:
  // Same as InitializeForNavigation() but instead of attaching to a URLRequest,
  // just creates a NavigationLoaderInterceptor and returns it.
  static std::unique_ptr<NavigationLoaderInterceptor>
  InitializeForNavigationNetworkService(
      const GURL& url,
      ResourceContext* resource_context,
      ServiceWorkerNavigationHandleCore* navigation_handle_core,
      storage::BlobStorageContext* blob_storage_context,
      bool skip_service_worker,
      ResourceType resource_type,
      blink::mojom::RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      bool is_parent_frame_secure,
      scoped_refptr<network::ResourceRequestBody> body,
      base::RepeatingCallback<WebContents*()> web_contents_getter);

  static std::unique_ptr<NavigationLoaderInterceptor> InitializeForSharedWorker(
      const network::ResourceRequest& resource_request,
      base::WeakPtr<ServiceWorkerProviderHost> host);

  // Attaches a newly created handler if the given |request| needs to
  // be handled by ServiceWorker.
  // TODO(kinuko): While utilizing UserData to attach data to URLRequest
  // has some precedence, it might be better to attach this handler in a more
  // explicit way within content layer, e.g. have ResourceRequestInfoImpl
  // own it.
  static void InitializeHandler(
      net::URLRequest* request,
      ServiceWorkerContextWrapper* context_wrapper,
      storage::BlobStorageContext* blob_storage_context,
      int process_id,
      int provider_id,
      bool skip_service_worker,
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchCredentialsMode credentials_mode,
      network::mojom::FetchRedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      blink::mojom::RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      scoped_refptr<network::ResourceRequestBody> body);

  // Returns the handler attached to |request|. This may return NULL
  // if no handler is attached.
  static ServiceWorkerRequestHandler* GetHandler(
      const net::URLRequest* request);

  // Creates a protocol interceptor for ServiceWorker.
  static std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor(
      ResourceContext* resource_context);

  // Returns true if the request falls into the scope of a ServiceWorker.
  // It's only reliable after the ServiceWorkerRequestHandler MaybeCreateJob
  // method runs to completion for this request. The AppCache handler uses
  // this to avoid colliding with ServiceWorkers.
  static bool IsControlledByServiceWorker(const net::URLRequest* request);

  // Returns the ServiceWorkerProviderHost the request is associated with.
  // Only valid after InitializeHandler has been called. Can return null.
  static ServiceWorkerProviderHost* GetProviderHost(
      const net::URLRequest* request);

  ~ServiceWorkerRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* context) = 0;

  // NavigationLoaderInterceptor overrides.
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;

 protected:
  ServiceWorkerRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  ResourceType resource_type_;

 private:
  static int user_data_key_;  // Only address is used.

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
