// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_handle.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "net/base/isolation_info.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace content {

ServiceWorkerMainResourceHandle::ServiceWorkerMainResourceHandle(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    ServiceWorkerAccessedCallback on_service_worker_accessed,
    std::string fetch_event_client_id,
    base::WeakPtr<ServiceWorkerClient> parent_service_worker_client)
    : parent_service_worker_client_(std::move(parent_service_worker_client)),
      fetch_event_client_id_(std::move(fetch_event_client_id)),
      service_worker_accessed_callback_(std::move(on_service_worker_accessed)),
      context_wrapper_(std::move(context_wrapper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerMainResourceHandle::~ServiceWorkerMainResourceHandle() = default;

void ServiceWorkerMainResourceHandle::set_service_worker_client(
    ScopedServiceWorkerClient scoped_service_worker_client,
    const net::IsolationInfo& isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!scoped_service_worker_client_);

  scoped_service_worker_client_ = std::make_unique<ScopedServiceWorkerClient>(
      std::move(scoped_service_worker_client));
  isolation_info_ = isolation_info;

  CHECK(service_worker_client());
}

base::WeakPtr<ServiceWorkerClient>
ServiceWorkerMainResourceHandle::service_worker_client() {
  if (!scoped_service_worker_client_) {
    return nullptr;
  }
  return scoped_service_worker_client_->AsWeakPtr();
}

std::optional<url::Origin>
ServiceWorkerMainResourceHandle::TopFrameOriginForInitializeForRequest(
    const network::ResourceRequest& resource_request) {
  // The storage key only has a top_level_site, not an origin, so we must
  // extract the origin from trusted_params.
  //
  // TODO(https://crbug.com/367755492): Investigate why we don't use
  // `ServiceWorkerMainResourceHandle::isolation_info_`. Currently
  // `TopFrameOriginForInitializeForRequest()` is a static method and doesn't
  // have direct access to `ServiceWorkerMainResourceHandle::isolation_info_`,
  // but this code has been historically using `ResourceRequest`'s isolation
  // info since before, even when this code had direct access to
  // `ServiceWorkerMainResourceHandle::isolation_info_`.
  return resource_request.trusted_params
             ? resource_request.trusted_params->isolation_info
                   .top_frame_origin()
             : std::nullopt;
}

bool ServiceWorkerMainResourceHandle::InitializeForRequest(
    const GURL& url,
    std::optional<url::Origin> top_frame_origin,
    const ServiceWorkerClient* client_for_prefetch) {
  CHECK(service_worker_client());

  // Update `isolation_info_`  to equal the net::IsolationInfo needed for any
  // service worker intercepting this request. Here, `isolation_info_` directly
  // corresponds to the StorageKey used to look up the service worker's
  // registration. That StorageKey will then be used later to recreate this
  // net::IsolationInfo for use by the ServiceWorker itself.
  url::Origin new_origin = url::Origin::Create(url);
  net::SiteForCookies new_site_for_cookies = isolation_info_.site_for_cookies();
  new_site_for_cookies.CompareWithFrameTreeOriginAndRevise(new_origin);
  auto new_isolation_info = net::IsolationInfo::Create(
      isolation_info_.request_type(),
      isolation_info_.top_frame_origin().value(), new_origin,
      new_site_for_cookies, isolation_info_.nonce());

  auto key = service_worker_client()->CalculateStorageKeyForUpdateUrls(
      url, new_isolation_info);

  if (client_for_prefetch) {
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));
    CHECK(service_worker_client()->IsContainerForWindowClient());
    CHECK(client_for_prefetch->IsContainerForWindowClient());
    // If top_frame_origin/key don't match, do not use the prefetch result.
    // `this` remains unmodified, and `InitializeForRequest` will be called
    // later in the non-prefetch code path.
    if (client_for_prefetch->top_frame_origin() != top_frame_origin ||
        client_for_prefetch->key() != key) {
      return false;
    }
  }

  isolation_info_ = std::move(new_isolation_info);

  // Clear old controller state if this is a redirect.
  service_worker_client()->SetControllerRegistration(
      nullptr,
      /*notify_controllerchange=*/false);

  // Update the service worker client. This is important to do this on every
  // requests/redirects before falling back to network below, so service worker
  // APIs still work even if the service worker is bypassed for request
  // interception.
  service_worker_client()->UpdateUrls(url, std::move(top_frame_origin),
                                      std::move(key));

  // Inherit the controller used for prefetching from `client_for_prefetch`.
  if (client_for_prefetch && client_for_prefetch->controller_registration()) {
    service_worker_client()->AddMatchingRegistration(
        client_for_prefetch->controller_registration());
    // `client_for_prefetch` shouldn't be in back forward cache because it's for
    // prefetch.
    CHECK(!client_for_prefetch->is_in_back_forward_cache());
    service_worker_client()->SetControllerRegistration(
        client_for_prefetch->controller_registration(),
        /*notify_controllerchange=*/false);
  }

  return true;
}

}  // namespace content
