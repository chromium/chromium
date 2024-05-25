// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_subresource_loader_params.h"

#include "content/browser/service_worker/service_worker_main_resource_handle.h"

namespace content {

SubresourceLoaderParams::SubresourceLoaderParams() = default;
SubresourceLoaderParams::~SubresourceLoaderParams() = default;

SubresourceLoaderParams::SubresourceLoaderParams(SubresourceLoaderParams&&) =
    default;
SubresourceLoaderParams& SubresourceLoaderParams::operator=(
    SubresourceLoaderParams&&) = default;

// static
void SubresourceLoaderParams::CheckWithMainResourceHandle(
    ServiceWorkerMainResourceHandle* handle,
    ServiceWorkerClient* service_worker_client_from_params,
    ServiceWorkerVersion* controller_at_params_creation) {
  const ServiceWorkerClient* service_worker_client_from_handle =
      handle ? handle->service_worker_client().get() : nullptr;

  // (1) The ServiceWorkerMainResourceHandle's client's controller at the
  //     time of `CommitResponse()` and
  // (2) the controller at the time of `SubresourceLoaderParams`
  // must be equal.
  // We've switched from (2) to (1) for creating
  // `blink::mojom::ControllerServiceWorkerInfoPtr` (crbug.com/336154571), but
  // also CHECK() the invariant here to confirm the switching was safe. We also
  // allow (1) is null while (2) is non-null, as (1) can be cleared e.g.
  // `ServiceWorkerClient::NotifyControllerLost()`, and in such cases switching
  // from (2) to (1) should be OK.
  //
  // TODO(crbug.com/336154571): Remove this once confirmed.
  if (service_worker_client_from_handle &&
      service_worker_client_from_handle->controller()) {
    CHECK_EQ(service_worker_client_from_handle->controller(),
             controller_at_params_creation);
  }

  // `ServiceWorkerMainResourceHandle::service_worker_client_` and
  // `SubresourceLoaderParams::service_worker_client` (and those plumbed from
  // `SubresourceLoaderParams`) should points to the same client (+ some
  // nullifying conditions).
  // TODO(crbug.com/336154571): Deduplicate them.

  // We also allow `SubresourceLoaderParams::service_worker_client_` is null if
  // the client doesn't have its controller at the time of
  // `SubresourceLoaderParams` creation.
  if (service_worker_client_from_handle &&
      !service_worker_client_from_handle->controller() &&
      !service_worker_client_from_params) {
    return;
  }

  // Otherwise, the two client pointers must be equal.
  CHECK_EQ(service_worker_client_from_handle,
           service_worker_client_from_params);
}

}  // namespace content
