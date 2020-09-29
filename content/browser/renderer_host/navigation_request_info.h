// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/referrer.h"
#include "net/base/isolation_info.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A struct to hold the parameters needed to start a navigation request in
// ResourceDispatcherHost. It is initialized on the UI thread, and then passed
// to the IO thread by a NavigationRequest object.
struct CONTENT_EXPORT NavigationRequestInfo {
  NavigationRequestInfo(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      const net::IsolationInfo& isolation_info,
      bool is_main_frame,
      bool parent_is_main_frame,
      bool are_ancestors_secure,
      int frame_tree_node_id,
      bool is_for_guests_only,
      bool report_raw_headers,
      bool is_prerendering,
      bool upgrade_if_insecure,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          blob_url_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      const base::UnguessableToken& devtools_frame_token,
      bool obey_origin_policy,
      net::HttpRequestHeaders cors_exempt_headers,
      network::mojom::ClientSecurityStatePtr client_security_state);
  NavigationRequestInfo(const NavigationRequestInfo& other) = delete;
  ~NavigationRequestInfo();

  mojom::CommonNavigationParamsPtr common_params;
  mojom::BeginNavigationParamsPtr begin_params;

  // Contains information used to prevent sharing information from a navigation
  // request across first party contexts. In particular, tracks the
  // SiteForCookies, which controls what site's SameSite cookies may be set,
  // NetworkIsolationKey, which is used to restrict sharing of network
  // resources, and how to update them across redirects, which is different for
  // main frames and subresources.
  const net::IsolationInfo isolation_info;

  const bool is_main_frame;
  const bool parent_is_main_frame;

  // Whether all ancestor frames of the frame that is navigating have a secure
  // origin. True for main frames.
  const bool are_ancestors_secure;

  const int frame_tree_node_id;

  const bool is_for_guests_only;

  const bool report_raw_headers;

  const bool is_prerendering;

  // If set to true, any HTTP redirects of this request will be upgraded to
  // HTTPS. This only applies for subframe navigations.
  const bool upgrade_if_insecure;

  // URLLoaderFactory to facilitate loading blob URLs.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      blob_url_loader_factory;

  const base::UnguessableToken devtools_navigation_token;

  const base::UnguessableToken devtools_frame_token;

  // If set, the network service will attempt to retrieve the appropriate origin
  // policy, if necessary, and attach it to the ResourceResponseHead.
  // Spec: https://wicg.github.io/origin-policy/
  const bool obey_origin_policy;

  const net::HttpRequestHeaders cors_exempt_headers;

  // Specifies the security state applying to the navigation. For iframes, this
  // is the security state of their parent. Nullptr otherwise.
  //
  // TODO(https://crbug.com/1129326): Set this for top-level navigation requests
  // too once the UX story is sorted out.
  const network::mojom::ClientSecurityStatePtr client_security_state;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_
