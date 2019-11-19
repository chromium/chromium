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
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
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

    bool operator==(const TrustedParams& other) const;

    net::NetworkIsolationKey network_isolation_key;
    mojom::UpdateNetworkIsolationKeyOnRedirect
        update_network_isolation_key_on_redirect =
            network::mojom::UpdateNetworkIsolationKeyOnRedirect::kDoNotUpdate;
    bool disable_secure_dns = false;
  };

  ResourceRequest();
  ResourceRequest(const ResourceRequest& request);
  ~ResourceRequest();

  bool EqualsForTesting(const ResourceRequest& request) const;
  bool SendsCookies() const;
  bool SavesCookies() const;

  // See comments in network.mojom.URLRequest in url_loader.mojom for details
  // of each field.
  std::string method = "GET";
  GURL url;
  GURL site_for_cookies;
  bool attach_same_site_cookies = false;
  bool update_first_party_url_on_redirect = false;
  base::Optional<url::Origin> request_initiator;
  base::Optional<url::Origin> isolated_world_origin;
  GURL referrer;
  net::URLRequest::ReferrerPolicy referrer_policy =
      net::URLRequest::NEVER_CLEAR_REFERRER;
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
  bool corb_excluded = false;
  mojom::RequestMode mode = mojom::RequestMode::kNoCors;
  mojom::CredentialsMode credentials_mode = mojom::CredentialsMode::kInclude;
  mojom::RedirectMode redirect_mode = mojom::RedirectMode::kFollow;
  std::string fetch_integrity;
  int fetch_request_context_type = 0;
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
  bool custom_proxy_use_alternate_proxy_list = false;
  base::Optional<base::UnguessableToken> fetch_window_id;
  base::Optional<std::string> devtools_request_id;
  bool is_signed_exchange_prefetch_cache_enabled = false;
  bool obey_origin_policy = false;
  base::Optional<base::UnguessableToken> recursive_prefetch_token;
  base::Optional<TrustedParams> trusted_params;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
