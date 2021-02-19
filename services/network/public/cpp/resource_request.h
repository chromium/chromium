// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_

#include <stdint.h>
#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/request_priority.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/auth_and_certificate_observer.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// Typemapped to network.mojom.URLRequest in url_loader.mojom.
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

    bool EqualsForTesting(const TrustedParams& trusted_params) const;

    net::IsolationInfo isolation_info;
    bool disable_secure_dns = false;
    bool has_user_activation = false;
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer;
    mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver>
        auth_cert_observer;
    mojom::ClientSecurityStatePtr client_security_state;
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
  ~ResourceRequest();

  bool EqualsForTesting(const ResourceRequest& request) const;
  bool SendsCookies() const;
  bool SavesCookies() const;

  // See comments in network.mojom.URLRequest in url_loader.mojom for details
  // of each field.
  std::string method = net::HttpRequestHeaders::kGetMethod;
  GURL url;
  net::SiteForCookies site_for_cookies;
  bool update_first_party_url_on_redirect = false;

  // SECURITY NOTE: |request_initiator| is a security-sensitive field.  Please
  // consult the doc comment for |request_initiator| in url_loader.mojom.
  base::Optional<url::Origin> request_initiator;

  base::Optional<url::Origin> isolated_world_origin;
  GURL referrer;
  net::ReferrerPolicy referrer_policy = net::ReferrerPolicy::NEVER_CLEAR;
  net::HttpRequestHeaders headers;
  net::HttpRequestHeaders cors_exempt_headers;
  int load_flags = 0;
  int resource_type = 0;
  net::RequestPriority priority = net::IDLE;
  bool should_reset_appcache = false;
  bool is_external_request = false;
  mojom::CorsPreflightPolicy cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  bool originated_from_service_worker = false;
  bool skip_service_worker = false;
  bool corb_detachable = false;
  mojom::RequestMode mode = mojom::RequestMode::kNoCors;
  mojom::CredentialsMode credentials_mode = mojom::CredentialsMode::kInclude;
  mojom::RedirectMode redirect_mode = mojom::RedirectMode::kFollow;
  std::string fetch_integrity;
  mojom::RequestDestination destination = mojom::RequestDestination::kEmpty;
  scoped_refptr<ResourceRequestBody> request_body;
  bool keepalive = false;
  bool has_user_gesture = false;
  bool enable_load_timing = false;
  bool enable_upload_progress = false;
  bool do_not_prompt_for_login = false;
  int render_frame_id = MSG_ROUTING_NONE;
  bool is_main_frame = false;
  int transition_type = 0;
  bool report_raw_headers = false;
  int previews_state = 0;
  bool upgrade_if_insecure = false;
  bool is_revalidating = false;
  base::Optional<base::UnguessableToken> throttling_profile_id;
  net::HttpRequestHeaders custom_proxy_pre_cache_headers;
  net::HttpRequestHeaders custom_proxy_post_cache_headers;
  base::Optional<base::UnguessableToken> fetch_window_id;
  base::Optional<std::string> devtools_request_id;
  base::Optional<std::string> devtools_stack_id;
  bool is_signed_exchange_prefetch_cache_enabled = false;
  bool is_fetch_like_api = false;
  bool is_favicon = false;
  bool obey_origin_policy = false;
  base::Optional<base::UnguessableToken> recursive_prefetch_token;
  base::Optional<TrustedParams> trusted_params;
  // |trust_token_params| uses a custom base::Optional-like type to make the
  // field trivially copyable; see OptionalTrustTokenParams's definition for
  // more context.
  OptionalTrustTokenParams trust_token_params;
  base::Optional<WebBundleTokenParams> web_bundle_token_params;
};

// This does not accept |kDefault| referrer policy.
COMPONENT_EXPORT(NETWORK_CPP_BASE)
net::ReferrerPolicy ReferrerPolicyForUrlRequest(
    mojom::ReferrerPolicy referrer_policy);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
