// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_

#include <optional>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "net/base/request_priority.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-forward.h"
#include "services/network/public/mojom/url_request.mojom-forward.h"
#include "services/network/public/mojom/web_bundle_handle.mojom-forward.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::SourceType, net::SourceStream::SourceType> {
  static network::mojom::SourceType ToMojom(net::SourceStream::SourceType type);
  static bool FromMojom(network::mojom::SourceType in,
                        net::SourceStream::SourceType* out);
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
  static bool allow_cookies_from_browser(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.allow_cookies_from_browser;
  }
  static bool include_request_cookies_with_response(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.include_request_cookies_with_response;
  }
  static mojo::PendingRemote<network::mojom::CookieAccessObserver>
  cookie_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.cookie_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .cookie_observer);
  }
  static mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
  trust_token_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.trust_token_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .trust_token_observer);
  }
  static mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  url_loader_network_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.url_loader_network_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .url_loader_network_observer);
  }
  static mojo::PendingRemote<network::mojom::DevToolsObserver>
  devtools_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.devtools_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .devtools_observer);
  }
  static const network::mojom::ClientSecurityStatePtr& client_security_state(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    return trusted_params.client_security_state;
  }
  static mojo::PendingRemote<network::mojom::AcceptCHFrameObserver>
  accept_ch_frame_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.accept_ch_frame_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .accept_ch_frame_observer);
  }
  static mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
  shared_dictionary_observer(
      const network::ResourceRequest::TrustedParams& trusted_params) {
    if (!trusted_params.shared_dictionary_observer) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::TrustedParams&>(trusted_params)
            .shared_dictionary_observer);
  }

  static bool Read(network::mojom::TrustedUrlRequestParamsDataView data,
                   network::ResourceRequest::TrustedParams* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::WebBundleTokenParamsDataView,
                 network::ResourceRequest::WebBundleTokenParams> {
  static const GURL& bundle_url(
      const network::ResourceRequest::WebBundleTokenParams& params) {
    return params.bundle_url;
  }
  static const base::UnguessableToken& token(
      const network::ResourceRequest::WebBundleTokenParams& params) {
    return params.token;
  }
  static mojo::PendingRemote<network::mojom::WebBundleHandle> web_bundle_handle(
      const network::ResourceRequest::WebBundleTokenParams& params) {
    if (!params.handle) {
      return mojo::NullRemote();
    }
    return std::move(
        const_cast<network::ResourceRequest::WebBundleTokenParams&>(params)
            .handle);
  }
  static int32_t render_process_id(
      const network::ResourceRequest::WebBundleTokenParams& params) {
    return params.render_process_id;
  }

  static bool Read(network::mojom::WebBundleTokenParamsDataView data,
                   network::ResourceRequest::WebBundleTokenParams* out);
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
  static bool update_first_party_url_on_redirect(
      const network::ResourceRequest& request) {
    return request.update_first_party_url_on_redirect;
  }
  static const std::optional<url::Origin>& request_initiator(
      const network::ResourceRequest& request) {
    return request.request_initiator;
  }
  static const std::vector<GURL> navigation_redirect_chain(
      const network::ResourceRequest& request) {
    return request.navigation_redirect_chain;
  }
  static const std::optional<url::Origin>& isolated_world_origin(
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
  static bool priority_incremental(const network::ResourceRequest& request) {
    return request.priority_incremental;
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
  static bool browsing_topics(const network::ResourceRequest& request) {
    return request.browsing_topics;
  }
  static bool ad_auction_headers(const network::ResourceRequest& request) {
    return request.ad_auction_headers;
  }
  static bool shared_storage_writable_eligible(
      const network::ResourceRequest& request) {
    return request.shared_storage_writable_eligible;
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
  static bool is_outermost_main_frame(const network::ResourceRequest& request) {
    return request.is_outermost_main_frame;
  }
  static int32_t transition_type(const network::ResourceRequest& request) {
    return request.transition_type;
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
  static const std::optional<base::UnguessableToken>& throttling_profile_id(
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
  static const std::optional<base::UnguessableToken>& fetch_window_id(
      const network::ResourceRequest& request) {
    return request.fetch_window_id;
  }
  static const std::optional<std::string>& devtools_request_id(
      const network::ResourceRequest& request) {
    return request.devtools_request_id;
  }
  static const std::optional<std::string>& devtools_stack_id(
      const network::ResourceRequest& request) {
    return request.devtools_stack_id;
  }
  static bool is_fetch_like_api(const network::ResourceRequest& request) {
    return request.is_fetch_like_api;
  }
  static bool is_fetch_later_api(const network::ResourceRequest& request) {
    return request.is_fetch_later_api;
  }
  static bool is_favicon(const network::ResourceRequest& request) {
    return request.is_favicon;
  }
  static network::mojom::RequestDestination original_destination(
      const network::ResourceRequest& request) {
    return request.original_destination;
  }
  static const std::optional<std::vector<net::SourceStream::SourceType>>&
  devtools_accepted_stream_types(const network::ResourceRequest& request) {
    return request.devtools_accepted_stream_types;
  }
  static const std::optional<network::ResourceRequest::TrustedParams>&
  trusted_params(const network::ResourceRequest& request) {
    return request.trusted_params;
  }
  static const std::optional<base::UnguessableToken>& recursive_prefetch_token(
      const network::ResourceRequest& request) {
    return request.recursive_prefetch_token;
  }
  static const network::mojom::TrustTokenParamsPtr& trust_token_params(
      const network::ResourceRequest& request) {
    return request.trust_token_params.as_ptr();
  }
  static const std::optional<network::ResourceRequest::WebBundleTokenParams>&
  web_bundle_token_params(const network::ResourceRequest& request) {
    return request.web_bundle_token_params;
  }
  static const std::optional<net::NetLogSource>& net_log_create_info(
      const network::ResourceRequest& request) {
    return request.net_log_create_info;
  }
  static const std::optional<net::NetLogSource>& net_log_reference_info(
      const network::ResourceRequest& request) {
    return request.net_log_reference_info;
  }
  static network::mojom::IPAddressSpace target_ip_address_space(
      const network::ResourceRequest& request) {
    return request.target_ip_address_space;
  }
  static net::StorageAccessApiStatus storage_access_api_status(
      const network::ResourceRequest& request) {
    return request.storage_access_api_status;
  }
  static network::mojom::AttributionSupport attribution_reporting_support(
      const network::ResourceRequest& request) {
    return request.attribution_reporting_support;
  }
  static network::mojom::AttributionReportingEligibility
  attribution_reporting_eligibility(const network::ResourceRequest& request) {
    return request.attribution_reporting_eligibility;
  }
  static const std::optional<base::UnguessableToken>&
  attribution_reporting_src_token(const network::ResourceRequest& request) {
    return request.attribution_reporting_src_token;
  }
  static bool is_ad_tagged(const network::ResourceRequest& request) {
    return request.is_ad_tagged;
  }
  static bool shared_dictionary_writer_enabled(
      const network::ResourceRequest& request) {
    return request.shared_dictionary_writer_enabled;
  }
  static network::mojom::IPAddressSpace required_ip_address_space(
      const network::ResourceRequest& request) {
    return request.required_ip_address_space;
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
    StructTraits<network::mojom::DataElementBytesDataView,
                 network::DataElementBytes> {
  static mojo_base::BigBufferView data(const network::DataElementBytes& data) {
    return mojo_base::BigBufferView(data.bytes());
  }

  static bool Read(network::mojom::DataElementBytesDataView data,
                   network::DataElementBytes* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::DataElementDataPipeDataView,
                 network::DataElementDataPipe> {
  static mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter(
      const network::DataElementDataPipe& data) {
    return data.CloneDataPipeGetter();
  }

  static bool Read(network::mojom::DataElementDataPipeDataView data,
                   network::DataElementDataPipe* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::DataElementChunkedDataPipeDataView,
                 network::DataElementChunkedDataPipe> {
  static mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
  data_pipe_getter(const network::DataElementChunkedDataPipe& data) {
    return const_cast<network::DataElementChunkedDataPipe&>(data)
        .ReleaseChunkedDataPipeGetter();
  }
  static bool read_only_once(const network::DataElementChunkedDataPipe& data) {
    return static_cast<bool>(data.read_only_once());
  }

  static bool Read(network::mojom::DataElementChunkedDataPipeDataView data,
                   network::DataElementChunkedDataPipe* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::DataElementFileDataView,
                 network::DataElementFile> {
  static base::FilePath path(const network::DataElementFile& data) {
    return data.path();
  }
  static uint64_t offset(const network::DataElementFile& data) {
    return data.offset();
  }
  static uint64_t length(const network::DataElementFile& data) {
    return data.length();
  }
  static base::Time expected_modification_time(
      const network::DataElementFile& data) {
    return data.expected_modification_time();
  }

  static bool Read(network::mojom::DataElementFileDataView data,
                   network::DataElementFile* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    UnionTraits<network::mojom::DataElementDataView, network::DataElement> {
  static network::mojom::DataElementDataView::Tag GetTag(
      const network::DataElement& data) {
    return data.type();
  }

  static const network::DataElementBytes& bytes(
      const network::DataElement& data) {
    return data.As<network::DataElementBytes>();
  }
  static const network::DataElementDataPipe& data_pipe(
      const network::DataElement& data) {
    return data.As<network::DataElementDataPipe>();
  }
  static const network::DataElementChunkedDataPipe& chunked_data_pipe(
      const network::DataElement& data) {
    return data.As<network::DataElementChunkedDataPipe>();
  }
  static const network::DataElementFile& file(
      const network::DataElement& data) {
    return data.As<network::DataElementFile>();
  }

  static bool Read(network::mojom::DataElementDataView data,
                   network::DataElement* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
