// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_mojom_traits.h"

#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "services/network/public/cpp/crash_keys.h"
#include "services/network/public/cpp/http_request_headers_mojom_traits.h"
#include "services/network/public/cpp/isolation_info_mojom_traits.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

network::mojom::RequestPriority
EnumTraits<network::mojom::RequestPriority, net::RequestPriority>::ToMojom(
    net::RequestPriority priority) {
  switch (priority) {
    case net::THROTTLED:
      return network::mojom::RequestPriority::kThrottled;
    case net::IDLE:
      return network::mojom::RequestPriority::kIdle;
    case net::LOWEST:
      return network::mojom::RequestPriority::kLowest;
    case net::LOW:
      return network::mojom::RequestPriority::kLow;
    case net::MEDIUM:
      return network::mojom::RequestPriority::kMedium;
    case net::HIGHEST:
      return network::mojom::RequestPriority::kHighest;
  }
  NOTREACHED();
  return static_cast<network::mojom::RequestPriority>(priority);
}

bool EnumTraits<network::mojom::RequestPriority, net::RequestPriority>::
    FromMojom(network::mojom::RequestPriority in, net::RequestPriority* out) {
  switch (in) {
    case network::mojom::RequestPriority::kThrottled:
      *out = net::THROTTLED;
      return true;
    case network::mojom::RequestPriority::kIdle:
      *out = net::IDLE;
      return true;
    case network::mojom::RequestPriority::kLowest:
      *out = net::LOWEST;
      return true;
    case network::mojom::RequestPriority::kLow:
      *out = net::LOW;
      return true;
    case network::mojom::RequestPriority::kMedium:
      *out = net::MEDIUM;
      return true;
    case network::mojom::RequestPriority::kHighest:
      *out = net::HIGHEST;
      return true;
  }

  NOTREACHED();
  *out = static_cast<net::RequestPriority>(in);
  return true;
}

network::mojom::URLRequestReferrerPolicy
EnumTraits<network::mojom::URLRequestReferrerPolicy,
           net::ReferrerPolicy>::ToMojom(net::ReferrerPolicy policy) {
  switch (policy) {
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::URLRequestReferrerPolicy::
          kClearReferrerOnTransitionFromSecureToInsecure;
    case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kReduceReferrerGranularityOnTransitionCrossOrigin;
    case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kOriginOnlyOnTransitionCrossOrigin;
    case net::ReferrerPolicy::NEVER_CLEAR:
      return network::mojom::URLRequestReferrerPolicy::kNeverClearReferrer;
    case net::ReferrerPolicy::ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::kOrigin;
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kClearReferrerOnTransitionCrossOrigin;
    case net::ReferrerPolicy::
        ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::URLRequestReferrerPolicy::
          kOriginClearOnTransitionFromSecureToInsecure;
    case net::ReferrerPolicy::NO_REFERRER:
      return network::mojom::URLRequestReferrerPolicy::kNoReferrer;
  }
  NOTREACHED();
  return static_cast<network::mojom::URLRequestReferrerPolicy>(policy);
}

bool EnumTraits<network::mojom::URLRequestReferrerPolicy, net::ReferrerPolicy>::
    FromMojom(network::mojom::URLRequestReferrerPolicy in,
              net::ReferrerPolicy* out) {
  switch (in) {
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionFromSecureToInsecure:
      *out = net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kReduceReferrerGranularityOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginOnlyOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kNeverClearReferrer:
      *out = net::ReferrerPolicy::NEVER_CLEAR;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kOrigin:
      *out = net::ReferrerPolicy::ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginClearOnTransitionFromSecureToInsecure:
      *out = net::ReferrerPolicy::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kNoReferrer:
      *out = net::ReferrerPolicy::NO_REFERRER;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<network::mojom::TrustedUrlRequestParamsDataView,
                  network::ResourceRequest::TrustedParams>::
    Read(network::mojom::TrustedUrlRequestParamsDataView data,
         network::ResourceRequest::TrustedParams* out) {
  if (!data.ReadIsolationInfo(&out->isolation_info)) {
    return false;
  }
  out->disable_secure_dns = data.disable_secure_dns();
  out->has_user_activation = data.has_user_activation();
  out->cookie_observer = data.TakeCookieObserver<
      mojo::PendingRemote<network::mojom::CookieAccessObserver>>();
  if (!data.ReadClientSecurityState(&out->client_security_state)) {
    return false;
  }
  return true;
}

bool StructTraits<
    network::mojom::URLRequestDataView,
    network::ResourceRequest>::Read(network::mojom::URLRequestDataView data,
                                    network::ResourceRequest* out) {
  if (!data.ReadMethod(&out->method)) {
    return false;
  }
  if (!data.ReadUrl(&out->url)) {
    network::debug::SetDeserializationCrashKeyString("url");
    return false;
  }
  if (!data.ReadSiteForCookies(&out->site_for_cookies) ||
      !data.ReadTrustedParams(&out->trusted_params)) {
    return false;
  }
  if (!data.ReadRequestInitiator(&out->request_initiator)) {
    network::debug::SetDeserializationCrashKeyString("request_initiator");
    return false;
  }
  if (!data.ReadIsolatedWorldOrigin(&out->isolated_world_origin)) {
    network::debug::SetDeserializationCrashKeyString("isolated_world_origin");
    return false;
  }
  if (!data.ReadReferrer(&out->referrer)) {
    network::debug::SetDeserializationCrashKeyString("referrer");
    return false;
  }
  if (!data.ReadReferrerPolicy(&out->referrer_policy) ||
      !data.ReadHeaders(&out->headers) ||
      !data.ReadCorsExemptHeaders(&out->cors_exempt_headers) ||
      !data.ReadPriority(&out->priority) ||
      !data.ReadCorsPreflightPolicy(&out->cors_preflight_policy) ||
      !data.ReadMode(&out->mode) ||
      !data.ReadCredentialsMode(&out->credentials_mode) ||
      !data.ReadRedirectMode(&out->redirect_mode) ||
      !data.ReadFetchIntegrity(&out->fetch_integrity) ||
      !data.ReadRequestBody(&out->request_body) ||
      !data.ReadThrottlingProfileId(&out->throttling_profile_id) ||
      !data.ReadFetchWindowId(&out->fetch_window_id) ||
      !data.ReadDevtoolsRequestId(&out->devtools_request_id) ||
      !data.ReadRecursivePrefetchToken(&out->recursive_prefetch_token)) {
    // Note that data.ReadTrustTokenParams is temporarily handled below.
    return false;
  }

  // Temporarily separated from the remainder of the deserialization in order to
  // help debug crbug.com/1062637.
  if (!data.ReadTrustTokenParams(&out->trust_token_params.as_ptr())) {
    // We don't return false here to avoid duplicate reports.
    out->trust_token_params = base::nullopt;
    base::debug::DumpWithoutCrashing();
  }

  out->force_ignore_site_for_cookies = data.force_ignore_site_for_cookies();
  out->update_first_party_url_on_redirect =
      data.update_first_party_url_on_redirect();
  out->load_flags = data.load_flags();
  out->resource_type = data.resource_type();
  out->should_reset_appcache = data.should_reset_appcache();
  out->is_external_request = data.is_external_request();
  out->originated_from_service_worker = data.originated_from_service_worker();
  out->skip_service_worker = data.skip_service_worker();
  out->corb_detachable = data.corb_detachable();
  out->corb_excluded = data.corb_excluded();
  out->destination = data.destination();
  out->keepalive = data.keepalive();
  out->has_user_gesture = data.has_user_gesture();
  out->enable_load_timing = data.enable_load_timing();
  out->enable_upload_progress = data.enable_upload_progress();
  out->do_not_prompt_for_login = data.do_not_prompt_for_login();
  out->render_frame_id = data.render_frame_id();
  out->is_main_frame = data.is_main_frame();
  out->transition_type = data.transition_type();
  out->report_raw_headers = data.report_raw_headers();
  out->previews_state = data.previews_state();
  out->upgrade_if_insecure = data.upgrade_if_insecure();
  out->is_revalidating = data.is_revalidating();
  out->is_signed_exchange_prefetch_cache_enabled =
      data.is_signed_exchange_prefetch_cache_enabled();
  out->obey_origin_policy = data.obey_origin_policy();
  return true;
}

bool StructTraits<network::mojom::URLRequestBodyDataView,
                  scoped_refptr<network::ResourceRequestBody>>::
    Read(network::mojom::URLRequestBodyDataView data,
         scoped_refptr<network::ResourceRequestBody>* out) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  if (!data.ReadElements(&(body->elements_)))
    return false;
  body->set_identifier(data.identifier());
  body->set_contains_sensitive_info(data.contains_sensitive_info());
  body->SetAllowHTTP1ForStreamingUpload(
      data.allow_http1_for_streaming_upload());
  *out = std::move(body);
  return true;
}

bool StructTraits<network::mojom::DataElementDataView, network::DataElement>::
    Read(network::mojom::DataElementDataView data, network::DataElement* out) {
  if (!data.ReadPath(&out->path_)) {
    network::debug::SetDeserializationCrashKeyString("data_element_path");
    return false;
  }
  if (!data.ReadBlobUuid(&out->blob_uuid_)) {
    network::debug::SetDeserializationCrashKeyString("data_element_blob_uuid");
    return false;
  }
  if (!data.ReadExpectedModificationTime(&out->expected_modification_time_)) {
    return false;
  }
  if (data.type() == network::mojom::DataElementType::kBytes) {
    if (!data.ReadBuf(&out->buf_))
      return false;
    if (data.length() != out->buf_.size())
      return false;
  }
  out->type_ = data.type();
  out->data_pipe_getter_ = data.TakeDataPipeGetter<
      mojo::PendingRemote<network::mojom::DataPipeGetter>>();
  out->chunked_data_pipe_getter_ = data.TakeChunkedDataPipeGetter<
      mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>>();
  out->offset_ = data.offset();
  out->length_ = data.length();
  return true;
}

}  // namespace mojo
