// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/request_priority.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/site_for_cookies_mojom_traits.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::RequestPriority, net::RequestPriority> {
  static network::mojom::RequestPriority ToMojom(net::RequestPriority priority);
  static bool FromMojom(network::mojom::RequestPriority in,
                        net::RequestPriority* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::URLRequestReferrerPolicy, net::ReferrerPolicy> {
  static network::mojom::URLRequestReferrerPolicy ToMojom(
      net::ReferrerPolicy policy);
  static bool FromMojom(network::mojom::URLRequestReferrerPolicy in,
                        net::ReferrerPolicy* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::TrustedUrlRequestParamsDataView,
                 network::ResourceRequest::TrustedParams> {
  static const net::IsolationInfo& isolation_info(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.isolation_info;
  }
  static bool disable_secure_dns(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.disable_secure_dns;
  }
  static bool has_user_activation(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.has_user_activation;
  }
  static mojo::PendingRemote<network::mojom::CookieAccessObserver>
  cookie_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.cookie_observer)
      return mojo::NullRemote();
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .cookie_observer);
  }
  static const network::mojom::ClientSecurityStatePtr& client_security_state(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.client_security_state;
  }

  static bool Read(network::mojom::TrustedUrlRequestParamsDataView data,
                   network::ResourceRequest::TrustedParams* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::URLRequestDataView, network::ResourceRequest> {
  static const std::string& method(const network::ResourceRequest& request) {
    return request.method;
  }
  static const GURL& url(const network::ResourceRequest& request) {
    return request.url;
  }
  static const net::SiteForCookies& site_for_cookies(
      const network::ResourceRequest& request) {
    return request.site_for_cookies;
  }
  static bool force_ignore_site_for_cookies(
      const network::ResourceRequest& request) {
    return request.force_ignore_site_for_cookies;
  }
  static bool update_first_party_url_on_redirect(
      const network::ResourceRequest& request) {
    return request.update_first_party_url_on_redirect;
  }
  static const base::Optional<url::Origin>& request_initiator(
      const network::ResourceRequest& request) {
    return request.request_initiator;
  }
  static const base::Optional<url::Origin>& isolated_world_origin(
      const network::ResourceRequest& request) {
    return request.isolated_world_origin;
  }
  static const GURL& referrer(const network::ResourceRequest& request) {
    return request.referrer;
  }
  static net::ReferrerPolicy referrer_policy(
      const network::ResourceRequest& request) {
    return request.referrer_policy;
  }
  static const net::HttpRequestHeaders& headers(
      const network::ResourceRequest& request) {
    return request.headers;
  }
  static const net::HttpRequestHeaders& cors_exempt_headers(
      const network::ResourceRequest& request) {
    return request.cors_exempt_headers;
  }
  static int32_t load_flags(const network::ResourceRequest& request) {
    return request.load_flags;
  }
  static int32_t resource_type(const network::ResourceRequest& request) {
    return request.resource_type;
  }
  static net::RequestPriority priority(
      const network::ResourceRequest& request) {
    return request.priority;
  }
  static bool should_reset_appcache(const network::ResourceRequest& request) {
    return request.should_reset_appcache;
  }
  static bool is_external_request(const network::ResourceRequest& request) {
    return request.is_external_request;
  }
  static network::mojom::CorsPreflightPolicy cors_preflight_policy(
      const network::ResourceRequest& request) {
    return request.cors_preflight_policy;
  }
  static bool originated_from_service_worker(
      const network::ResourceRequest& request) {
    return request.originated_from_service_worker;
  }
  static bool skip_service_worker(const network::ResourceRequest& request) {
    return request.skip_service_worker;
  }
  static bool corb_detachable(const network::ResourceRequest& request) {
    return request.corb_detachable;
  }
  static bool corb_excluded(const network::ResourceRequest& request) {
    return request.corb_excluded;
  }
  static network::mojom::RequestMode mode(
      const network::ResourceRequest& request) {
    return request.mode;
  }
  static network::mojom::CredentialsMode credentials_mode(
      const network::ResourceRequest& request) {
    return request.credentials_mode;
  }
  static network::mojom::RedirectMode redirect_mode(
      const network::ResourceRequest& request) {
    return request.redirect_mode;
  }
  static const std::string& fetch_integrity(
      const network::ResourceRequest& request) {
    return request.fetch_integrity;
  }
  static network::mojom::RequestDestination destination(
      const network::ResourceRequest& request) {
    return request.destination;
  }
  static const scoped_refptr<network::ResourceRequestBody>& request_body(
      const network::ResourceRequest& request) {
    return request.request_body;
  }
  static bool keepalive(const network::ResourceRequest& request) {
    return request.keepalive;
  }
  static bool has_user_gesture(const network::ResourceRequest& request) {
    return request.has_user_gesture;
  }
  static bool enable_load_timing(const network::ResourceRequest& request) {
    return request.enable_load_timing;
  }
  static bool enable_upload_progress(const network::ResourceRequest& request) {
    return request.enable_upload_progress;
  }
  static bool do_not_prompt_for_login(const network::ResourceRequest& request) {
    return request.do_not_prompt_for_login;
  }
  static int32_t render_frame_id(const network::ResourceRequest& request) {
    return request.render_frame_id;
  }
  static bool is_main_frame(const network::ResourceRequest& request) {
    return request.is_main_frame;
  }
  static int32_t transition_type(const network::ResourceRequest& request) {
    return request.transition_type;
  }
  static bool report_raw_headers(const network::ResourceRequest& request) {
    return request.report_raw_headers;
  }
  static int32_t previews_state(const network::ResourceRequest& request) {
    return request.previews_state;
  }
  static bool upgrade_if_insecure(const network::ResourceRequest& request) {
    return request.upgrade_if_insecure;
  }
  static bool is_revalidating(const network::ResourceRequest& request) {
    return request.is_revalidating;
  }
  static const base::Optional<base::UnguessableToken>& throttling_profile_id(
      const network::ResourceRequest& request) {
    return request.throttling_profile_id;
  }
  static const net::HttpRequestHeaders& custom_proxy_pre_cache_headers(
      const network::ResourceRequest& request) {
    return request.custom_proxy_pre_cache_headers;
  }
  static const net::HttpRequestHeaders& custom_proxy_post_cache_headers(
      const network::ResourceRequest& request) {
    return request.custom_proxy_post_cache_headers;
  }
  static const base::Optional<base::UnguessableToken>& fetch_window_id(
      const network::ResourceRequest& request) {
    return request.fetch_window_id;
  }
  static const base::Optional<std::string>& devtools_request_id(
      const network::ResourceRequest& request) {
    return request.devtools_request_id;
  }
  static bool is_signed_exchange_prefetch_cache_enabled(
      const network::ResourceRequest& request) {
    return request.is_signed_exchange_prefetch_cache_enabled;
  }
  static bool obey_origin_policy(const network::ResourceRequest& request) {
    return request.obey_origin_policy;
  }
  static const base::Optional<network::ResourceRequest::TrustedParams>&
  trusted_params(const network::ResourceRequest& request) {
    return request.trusted_params;
  }
  static const base::Optional<base::UnguessableToken>& recursive_prefetch_token(
      const network::ResourceRequest& request) {
    return request.recursive_prefetch_token;
  }
  static const network::mojom::TrustTokenParamsPtr& trust_token_params(
      const network::ResourceRequest& request) {
    return request.trust_token_params.as_ptr();
  }

  static bool Read(network::mojom::URLRequestDataView data,
                   network::ResourceRequest* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::URLRequestBodyDataView,
                 scoped_refptr<network::ResourceRequestBody>> {
  static bool IsNull(const scoped_refptr<network::ResourceRequestBody>& r) {
    return !r;
  }

  static void SetToNull(scoped_refptr<network::ResourceRequestBody>* output) {
    output->reset();
  }

  static const std::vector<network::DataElement>& elements(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return *r->elements();
  }

  static uint64_t identifier(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->identifier_;
  }

  static bool contains_sensitive_info(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->contains_sensitive_info_;
  }

  static bool allow_http1_for_streaming_upload(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->allow_http1_for_streaming_upload_;
  }

  static bool Read(network::mojom::URLRequestBodyDataView data,
                   scoped_refptr<network::ResourceRequestBody>* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::DataElementDataView, network::DataElement> {
  static const network::mojom::DataElementType& type(
      const network::DataElement& element) {
    return element.type_;
  }
  static const std::vector<uint8_t>& buf(const network::DataElement& element) {
    return element.buf_;
  }
  static const base::FilePath& path(const network::DataElement& element) {
    return element.path_;
  }
  static const std::string& blob_uuid(const network::DataElement& element) {
    return element.blob_uuid_;
  }
  static mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter(
      const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kDataPipe)
      return mojo::NullRemote();
    return element.CloneDataPipeGetter();
  }
  static mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
  chunked_data_pipe_getter(const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kChunkedDataPipe &&
        element.type_ != network::mojom::DataElementType::kReadOnceStream)
      return mojo::NullRemote();
    return const_cast<network::DataElement&>(element)
        .ReleaseChunkedDataPipeGetter();
  }
  static uint64_t offset(const network::DataElement& element) {
    return element.offset_;
  }
  static uint64_t length(const network::DataElement& element) {
    return element.length_;
  }
  static const base::Time& expected_modification_time(
      const network::DataElement& element) {
    return element.expected_modification_time_;
  }

  static bool Read(network::mojom::DataElementDataView data,
                   network::DataElement* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
