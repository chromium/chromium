// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_navigation_loader.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace content {

class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Handles main resource requests for service worker clients (documents and
// shared workers).
//
// TODO(crbug.com/824858): Merge into ServiceWorkerNavigationLoaderInterceptor
// after the service worker core thread changes to the UI thread.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler final {
 public:
  // If |skip_service_worker| is true, service workers are bypassed for
  // request interception.
  ServiceWorkerControlleeRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ResourceType resource_type,
      bool skip_service_worker);
  ~ServiceWorkerControlleeRequestHandler();

  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  using ServiceWorkerLoaderCallback =
      base::OnceCallback<void(SingleRequestURLLoaderFactory::RequestHandler)>;
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_request,
      BrowserContext* browser_context,
      ResourceContext* resource_context,
      ServiceWorkerLoaderCallback callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback);
  // Returns params with the ControllerServiceWorkerInfoPtr if we have found
  // a matching controller service worker for the |request| that is given
  // to MaybeCreateLoader(). Otherwise this returns base::nullopt.
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams();

  // Does all initialization of |provider_host_| for a request.
  bool InitializeProvider(const network::ResourceRequest& tentative_request);

  // Exposed for testing.
  ServiceWorkerNavigationLoader* loader() {
    return loader_wrapper_ ? loader_wrapper_->get() : nullptr;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);

  void ContinueWithRegistration(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithActivatedVersion(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version);

  // For forced update.
  void DidUpdateRegistration(
      scoped_refptr<ServiceWorkerRegistration> original_registration,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  void OnUpdatedVersionStatusChanged(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version);

  // Sets |job_| to nullptr, and clears all extra response info associated with
  // that job, except for timing information.
  void ClearJob();

  void CompleteWithoutLoader();

  // Schedules a service worker update to occur shortly after the page and its
  // initial subresources load, if this handler was for a navigation.
  void MaybeScheduleUpdate();

  const base::WeakPtr<ServiceWorkerContextCore> context_;
  const base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const ResourceType resource_type_;

  // If true, service workers are bypassed for request interception.
  const bool skip_service_worker_;

  std::unique_ptr<ServiceWorkerNavigationLoaderWrapper> loader_wrapper_;
  BrowserContext* browser_context_;
  ResourceContext* resource_context_;
  GURL stripped_url_;
  bool force_update_started_;
  base::TimeTicks registration_lookup_start_time_;

  ServiceWorkerLoaderCallback loader_callback_;
  NavigationLoaderInterceptor::FallbackCallback fallback_callback_;

  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
