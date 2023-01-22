// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"

#include "base/notreached.h"
#include "services/network/public/mojom/url_response_head.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/loader/fetch/delivery_type_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
using network::mojom::blink::NavigationDeliveryType;

namespace {
const AtomicString& GetDeliveryType(
    NavigationDeliveryType navigation_delivery_type,
    mojom::blink::CacheState cache_state) {
  switch (navigation_delivery_type) {
    case NavigationDeliveryType::kDefault:
      return cache_state == mojom::blink::CacheState::kNone
                 ? g_empty_atom
                 : delivery_type_names::kCache;
    case NavigationDeliveryType::kNavigationalPrefetch:
      return delivery_type_names::kNavigationalPrefetch;
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}
}  // namespace

ResourceTimingInfo::ResourceTimingInfo(
    const mojom::blink::ResourceTimingInfo& info)
    : name_(info.name),
      render_blocking_status_(info.render_blocking_status
                                  ? RenderBlockingStatusType::kBlocking
                                  : RenderBlockingStatusType::kNonBlocking),
      content_type_(info.content_type),
      context_type_(info.context_type),
      request_destination_(info.request_destination),
      load_response_end_(info.response_end),
      response_status_(info.response_status),
      negative_allowed_(info.allow_negative_values),
      delivery_type_(
          GetDeliveryType(NavigationDeliveryType::kDefault, info.cache_state)),
      alpn_negotiated_protocol_(
          static_cast<String>(info.alpn_negotiated_protocol)),
      connection_info_(static_cast<String>(info.connection_info)),
      resource_load_timing_(ResourceLoadTiming::FromMojo(info.timing.get())),
      last_redirect_end_time_(info.last_redirect_end_time),
      cache_state_(info.cache_state),
      encoded_body_size_(info.encoded_body_size),
      decoded_body_size_(info.decoded_body_size),
      did_reuse_connection_(info.did_reuse_connection),
      allow_timing_details_(info.allow_timing_details),
      allow_redirect_details_(info.allow_redirect_details),
      is_secure_transport_(info.is_secure_transport) {}

void ResourceTimingInfo::AddRedirect(const ResourceResponse& redirect_response,
                                     const KURL& new_url) {
  const ResourceLoadTiming* timing = redirect_response.GetResourceLoadTiming();
  if (timing) {
    last_redirect_end_time_ = timing->ReceiveHeadersEnd();
  }
  if (!SecurityOrigin::Create(new_url)->CanAccess(
          SecurityOrigin::Create(redirect_response.CurrentRequestUrl()))) {
    has_cross_origin_redirects_ = true;
  }
}

void ResourceTimingInfo::SetDeliveryType(
    const network::mojom::NavigationDeliveryType delivery_type,
    mojom::blink::CacheState cache_state) {
  delivery_type_ = GetDeliveryType(delivery_type, cache_state);
}
}  // namespace blink
