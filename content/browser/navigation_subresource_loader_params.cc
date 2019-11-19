// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_subresource_loader_params.h"

namespace content {

SubresourceLoaderParams::SubresourceLoaderParams() = default;
SubresourceLoaderParams::~SubresourceLoaderParams() = default;

SubresourceLoaderParams::SubresourceLoaderParams(
    SubresourceLoaderParams&& other) {
  *this = std::move(other);
}

SubresourceLoaderParams& SubresourceLoaderParams::operator=(
    SubresourceLoaderParams&& other) {
  pending_appcache_loader_factory =
      std::move(other.pending_appcache_loader_factory);
  controller_service_worker_info =
      std::move(other.controller_service_worker_info);
  controller_service_worker_object_host =
      other.controller_service_worker_object_host;
  prefetched_signed_exchanges = std::move(other.prefetched_signed_exchanges);
  return *this;
}

}  // namespace content
