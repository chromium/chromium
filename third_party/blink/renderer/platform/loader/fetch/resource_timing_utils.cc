// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "resource_response.h"
#include "services/network/public/mojom/load_timing_info.mojom-blink.h"
#include "services/network/public/mojom/url_response_head.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/delivery_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

Vector<mojom::blink::ServerTimingInfoPtr>
ParseServerTimingFromHeaderValueToMojo(const String& value) {
  std::unique_ptr<ServerTimingHeaderVector> headers =
      ParseServerTimingHeader(value);
  Vector<mojom::blink::ServerTimingInfoPtr> result;
  result.reserve(headers->size());
  for (const auto& header : *headers) {
    result.emplace_back(mojom::blink::ServerTimingInfo::New(
        header->Name(), header->Duration(), header->Description()));
  }
  return result;
}

}  // namespace

mojom::blink::ResourceTimingInfoPtr CreateResourceTimingInfo(
    base::TimeTicks start_time,
    const KURL& initial_url,
    const ResourceResponse* response) {
  mojom::blink::ResourceTimingInfoPtr info =
      mojom::blink::ResourceTimingInfo::New();
  info->start_time = start_time;
  info->name = initial_url;
  info->response_end = base::TimeTicks::Now();
  if (!response) {
    return info;
  }

  if (response->TimingAllowPassed()) {
    info->allow_timing_details = true;
    info->server_timing = ParseServerTimingFromHeaderValueToMojo(
        response->HttpHeaderField(http_names::kServerTiming));
    info->cache_state = response->CacheState();
    info->alpn_negotiated_protocol = response->AlpnNegotiatedProtocol().IsNull()
                                         ? g_empty_string
                                         : response->AlpnNegotiatedProtocol();
    info->connection_info = response->ConnectionInfoString().IsNull()
                                ? g_empty_string
                                : response->ConnectionInfoString();

    info->did_reuse_connection = response->ConnectionReused();
    // Use SecurityOrigin::Create to handle cases like blob:https://.
    info->is_secure_transport = base::Contains(
        url::GetSecureSchemes(),
        SecurityOrigin::Create(response->ResponseUrl())->Protocol().Ascii());
    info->timing = response->GetResourceLoadTiming()
                       ? response->GetResourceLoadTiming()->ToMojo()
                       : nullptr;
  } else {
    // [spec] https://fetch.spec.whatwg.org/#create-an-opaque-timing-info

    // Service worker timing for subresources is always same-origin
    // TODO: This doesn't match the spec, but probably the spec needs to be
    // changed. Opened https://github.com/whatwg/fetch/issues/1597
    if (response->GetResourceLoadTiming()) {
      ResourceLoadTiming* timing = response->GetResourceLoadTiming();
      info->timing = network::mojom::blink::LoadTimingInfo::New();
      info->timing->service_worker_start_time = timing->WorkerStart();
      info->timing->service_worker_ready_time = timing->WorkerReady();
      info->timing->service_worker_fetch_start = timing->WorkerFetchStart();
    }
  }

  info->service_worker_router_info =
      response->GetServiceWorkerRouterInfo()
          ? response->GetServiceWorkerRouterInfo()->ToMojo()
          : nullptr;

  bool allow_response_details = response->IsCorsSameOrigin();

  info->content_type = g_empty_string;

  if (allow_response_details) {
    info->response_status = response->HttpStatusCode();
    if (!response->HttpContentType().IsNull()) {
      info->content_type = MinimizedMIMEType(response->HttpContentType());
    }
  }

  bool expose_body_sizes =
      RuntimeEnabledFeatures::ResourceTimingUseCORSForBodySizesEnabled()
          ? allow_response_details
          : info->allow_timing_details;

  if (expose_body_sizes && response) {
    info->encoded_body_size = response->EncodedBodyLength();
    info->decoded_body_size = response->DecodedBodyLength();
    info->service_worker_response_source =
        response->GetServiceWorkerResponseSource();
  }

  return info;
}

}  // namespace blink
