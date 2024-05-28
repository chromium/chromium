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
    ServiceWorkerClient* service_worker_client_from_params) {
  const ServiceWorkerClient* service_worker_client_from_handle =
      handle ? handle->service_worker_client().get() : nullptr;

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
