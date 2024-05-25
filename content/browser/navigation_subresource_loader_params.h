// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
#define CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"

namespace content {

class ServiceWorkerClient;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerVersion;

// For NetworkService glues:
// Navigation parameters that are necessary to set-up a subresource loader
// for the frame that is going to be created by the navigation.
// Passed from the browser to the renderer when the navigation commits when
// NetworkService or its glue code for relevant features is enabled.
struct CONTENT_EXPORT SubresourceLoaderParams {
  SubresourceLoaderParams();
  ~SubresourceLoaderParams();

  SubresourceLoaderParams(SubresourceLoaderParams&& other);
  SubresourceLoaderParams& operator=(SubresourceLoaderParams&& other);

  // The service worker client corresponding to the to-be-created global scope.
  // This is mainly used to create
  // `blink::mojom::ControllerServiceWorkerInfoPtr` from its controller, to
  // indicate the controlling service worker (if any) for subresource loading.
  // The controller of `service_worker_client` should remain the same as the
  // service worker intercepted the main resource request (if any) unless the
  // service worker has been lost before navigation commit, so we don't keep the
  // controller information separately here.
  base::WeakPtr<ServiceWorkerClient> service_worker_client;

  // The controller at the time of `SubresourceLoaderParams` creation, to
  // CHECK() that it doesn't change before commit.
  // TODO(crbug.com/336154571): Remove this once we confirm the related CHECK()s
  // don't crash.
  base::WeakPtr<ServiceWorkerVersion> controller_at_params_creation;

  // When signed exchanges were prefetched in the previous page and were stored
  // to the PrefetchedSignedExchangeCache, and the main resource for the
  // navigation was served from the cache, |prefetched_signed_exchanges|
  // contains the all prefetched signed exchanges and they will be passed to the
  // renderer.
  std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>
      prefetched_signed_exchanges;

  // Should be called at the time of `ServiceWorkerClient::CommitResponse()` to
  // check some invariants (see implementation for details).
  // `service_worker_client_from_params` and `controller_at_params_creation`
  // come from `SubresourceLoaderParams`.
  static void CheckWithMainResourceHandle(
      ServiceWorkerMainResourceHandle* handle,
      ServiceWorkerClient* service_worker_client_from_params,
      ServiceWorkerVersion* controller_at_params_creation);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
