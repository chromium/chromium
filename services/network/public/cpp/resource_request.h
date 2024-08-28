// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/debug/crash_logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/request_priority.h"
#include "net/cookies/site_for_cookies.h"
#include "net/filter/source_stream.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_source.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/public/mojom/url_request.mojom-forward.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// Typemapped to network.mojom.URLRequest in url_request.mojom.
//
// Note: Please revise EqualsForTesting accordingly on any updates to this
// struct.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) ResourceRequest {
  // Typemapped to network.mojom.TrustedUrlRequestParams, see comments there
  // for details of each field.
  //
  // TODO(mmenke):  There are likely other fields that should be moved into this
  // class.
  struct COMPONENT_EXPORT(NETWORK_CPP_BASE) TrustedParams {
    TrustedParams();
    ~TrustedParams();
    // TODO(altimin): Make this move-only to avoid cloning mojo interfaces.
    TrustedParams(const TrustedParams& params);
    TrustedParams& operator=(const TrustedParams& other);
    TrustedParams(TrustedParams&& other);
    TrustedParams& operator=(TrustedParams&& other);

    bool EqualsForTesting(const TrustedParams& other) const;

    net::IsolationInfo isolation_info;
    bool disable_secure_dns = false;
    bool has_user_activation = false;
    bool allow_cookies_from_browser = false;
    bool include_request_cookies_with_response = false;
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer;
    mojo::PendingRemote<mojom::TrustTokenAccessObserver> trust_token_observer;
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer;
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
    mojom::ClientSecurityStatePtr client_security_state;
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer;
    mojo::PendingRemote<mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer;
  };

  // Typemapped to network.mojom.WebBundleTokenParams, see comments there
  // for details of each field.
  struct COMPONENT_EXPORT(NETWORK_CPP_BASE) WebBundleTokenParams {
    WebBundleTokenParams();
    ~WebBundleTokenParams();
    // Define a non-default copy-constructor because:
    // 1. network::ResourceRequest has a requirement that all of
    //    the members be trivially copyable.
    // 2. mojo::PendingRemote is non-copyable.
    WebBundleTokenParams(const WebBundleTokenParams& params);
    WebBundleTokenParams& operator=(const WebBundleTokenParams& other);

    WebBundleTokenParams(const GURL& bundle_url,
                         const base::UnguessableToken& token,
                         mojo::PendingRemote<mojom::WebBundleHandle> handle);
    WebBundleTokenParams(const GURL& bundle_url,
                         const base::UnguessableToken& token,
                         int32_t render_process_id);

    // For testing. Regarding the equality of |handle|, |this| equals |other| if
    // both |handle| exists, or neither exists, because we cannot test the
    // equality of two mojo handles.
    bool EqualsForTesting(const WebBundleTokenParams& other) const;

    mojo::PendingRemote<mojom::WebBundleHandle> CloneHandle() const;

    GURL bundle_url;
    base::UnguessableToken token;
    mojo::PendingRemote<mojom::WebBundleHandle> handle;
    int32_t render_process_id = -1;
  };

  ResourceRequest();
  ResourceRequest(const ResourceRequest& request);
  ResourceRequest& operator=(const ResourceRequest& other);
  ResourceRequest(ResourceRequest&& other);
  ResourceRequest& operator=(ResourceRequest&& other);

  ~ResourceRequest();

  bool EqualsForTesting(const ResourceRequest& request) const;
  bool SendsCookies() const;
  bool SavesCookies() const;

  // See comments in network.mojom.URLRequest in url_request.mojom for details
  // of each field.
  std::string method = net::HttpRequestHeaders::kGetMethod;
  GURL url;
  net::SiteForCookies site_for_cookies;
  bool update_first_party_url_on_redirect = false;

  // SECURITY NOTE: |request_initiator| is a security-sensitive field.  Please
  // consult the doc comment for |request_initiator| in url_request.mojom.
  std::optional<url::Origin> request_initiator;

  // TODO(crbug.com/40137011): Remove the `isolated_world_origin` field
  // once Chrome Platform Apps are gone.
  std::optional<url::Origin> isolated_world_origin;

  // The chain of URLs seen during navigation redirects.  This should only
  // contain values if the mode is `RedirectMode::kNavigate`.
  std::vector<GURL> navigation_redirect_chain;

  GURL referrer;
  net::ReferrerPolicy referrer_policy = net::ReferrerPolicy::NEVER_CLEAR;
  net::HttpRequestHeaders headers;
  net::HttpRequestHeaders cors_exempt_headers;
  int load_flags = 0;
  // Note: kMainFrame is used only for outermost main frames, i.e. fenced
  // frames are considered a kSubframe for ResourceType.
  int resource_type = 0;
  net::RequestPriority priority = net::IDLE;
  bool priority_incremental = net::kDefaultPriorityIncremental;
  mojom::CorsPreflightPolicy cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  bool originated_from_service_worker = false;
  bool skip_service_worker = false;
  // `kNoCors` mode is the default request mode for legacy reasons, however this
  // mode is highly discouraged for new requests made on the web platform;
  // please consider using another mode like `kCors` instead, and only use
  // `kNoCors` with strong rationale and approval from security experts. See
  // https://fetch.spec.whatwg.org/#concept-request-mode.
  mojom::RequestMode mode = mojom::RequestMode::kNoCors;
  mojom::IPAddressSpace required_ip_address_space =
      mojom::IPAddressSpace::kUnknown;
  mojom::CredentialsMode credentials_mode = mojom::CredentialsMode::kInclude;
  mojom::RedirectMode redirect_mode = mojom::RedirectMode::kFollow;
  // Exposed as Request.integrity in Service Workers
  std::string fetch_integrity;
  mojom::RequestDestination destination = mojom::RequestDestination::kEmpty;
  mojom::RequestDestination original_destination =
      mojom::RequestDestination::kEmpty;
  scoped_refptr<ResourceRequestBody> request_body;
  bool keepalive = false;
  bool browsing_topics = false;
  bool ad_auction_headers = false;
  bool shared_storage_writable_eligible = false;
  bool has_user_gesture = false;
  bool enable_load_timing = false;
  bool enable_upload_progress = false;
  bool do_not_prompt_for_login = false;
  bool is_outermost_main_frame = false;
  int transition_type = 0;
  int previews_state = 0;
  bool upgrade_if_insecure = false;
  bool is_revalidating = false;
  std::optional<base::UnguessableToken> throttling_profile_id;
  net::HttpRequestHeaders custom_proxy_pre_cache_headers;
  net::HttpRequestHeaders custom_proxy_post_cache_headers;
  std::optional<base::UnguessableToken> fetch_window_id;
  std::optional<std::string> devtools_request_id;
  std::optional<std::string> devtools_stack_id;
  bool is_fetch_like_api = false;
  bool is_fetch_later_api = false;
  bool is_favicon = false;
  std::optional<base::UnguessableToken> recursive_prefetch_token;
  std::optional<TrustedParams> trusted_params;
  // |trust_token_params| uses a custom std::optional-like type to make the
  // field trivially copyable; see OptionalTrustTokenParams's definition for
  // more context.
  OptionalTrustTokenParams trust_token_params;
  std::optional<WebBundleTokenParams> web_bundle_token_params;
  // If not null, the network service will not advertise any stream types
  // (via Accept-Encoding) that are not listed. Also, it will not attempt
  // decoding any non-listed stream types.
  std::optional<std::vector<net::SourceStream::SourceType>>
      devtools_accepted_stream_types;
  std::optional<net::NetLogSource> net_log_create_info;
  std::optional<net::NetLogSource> net_log_reference_info;
  mojom::IPAddressSpace target_ip_address_space =
      mojom::IPAddressSpace::kUnknown;
  net::StorageAccessApiStatus storage_access_api_status =
      net::StorageAccessApiStatus::kNone;
  network::mojom::AttributionSupport attribution_reporting_support =
      network::mojom::AttributionSupport::kUnset;
  mojom::AttributionReportingEligibility attribution_reporting_eligibility =
      mojom::AttributionReportingEligibility::kUnset;
  bool shared_dictionary_writer_enabled = false;
  std::optional<base::UnguessableToken> attribution_reporting_src_token;
  bool is_ad_tagged = false;
  std::optional<base::UnguessableToken> prefetch_token;
};

// This does not accept |kDefault| referrer policy.
COMPONENT_EXPORT(NETWORK_CPP_BASE)
net::ReferrerPolicy ReferrerPolicyForUrlRequest(
    mojom::ReferrerPolicy referrer_policy);

namespace debug {

class COMPONENT_EXPORT(NETWORK_CPP_BASE) ScopedResourceRequestCrashKeys {
 public:
  explicit ScopedResourceRequestCrashKeys(
      const network::ResourceRequest& request);
  ~ScopedResourceRequestCrashKeys();

  ScopedResourceRequestCrashKeys(const ScopedResourceRequestCrashKeys&) =
      delete;
  ScopedResourceRequestCrashKeys& operator=(
      const ScopedResourceRequestCrashKeys&) = delete;

 private:
  base::debug::ScopedCrashKeyString url_;
  url::debug::ScopedOriginCrashKey request_initiator_;
  base::debug::ScopedCrashKeyString resource_type_;
};

}  // namespace debug
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
