// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/devtools_observer_util.h"

#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/service_worker_router_info.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

mojom::URLResponseHeadDevToolsInfoPtr ExtractDevToolsInfo(
    const mojom::URLResponseHead& head) {
  return network::mojom::URLResponseHeadDevToolsInfo::New(
      head.response_time, head.headers, head.mime_type, head.charset,
      head.load_timing, head.cert_status, head.encoded_data_length,
      head.was_in_prefetch_cache, head.was_fetched_via_service_worker,
      head.cache_storage_cache_name, head.alpn_negotiated_protocol,
      head.alternate_protocol_usage, head.was_fetched_via_spdy,
      head.service_worker_response_source,
      head.service_worker_router_info.Clone(), head.ssl_info,
      head.remote_endpoint, head.emitted_extra_info);
}

mojom::URLRequestDevToolsInfoPtr ExtractDevToolsInfo(
    const ResourceRequest& request) {
  return network::mojom::URLRequestDevToolsInfo::New(
      request.method, request.url, request.priority, request.referrer_policy,
      request.trust_token_params ? request.trust_token_params->Clone()
                                 : nullptr,
      request.has_user_gesture, request.resource_type);
}

}  // namespace network
