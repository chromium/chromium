// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/referrer.h"
#include "net/base/isolation_info.h"
#include "net/filter/source_stream.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A struct to hold the parameters needed to start a navigation request in
// ResourceDispatcherHost. It is initialized on the UI thread, and then passed
// to the IO thread by a NavigationRequest object.
struct CONTENT_EXPORT NavigationRequestInfo {
  NavigationRequestInfo(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      network::mojom::WebSandboxFlags sandbox_flags,
      const net::IsolationInfo& isolation_info,
      bool is_primary_main_frame,
      bool is_main_frame,
      bool are_ancestors_secure,
      int frame_tree_node_id,
      bool report_raw_headers,
      bool upgrade_if_insecure,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          blob_url_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      const base::UnguessableToken& devtools_frame_token,
      bool obey_origin_policy,
      net::HttpRequestHeaders cors_exempt_headers,
      network::mojom::ClientSecurityStatePtr client_security_state,
      const absl::optional<std::vector<net::SourceStream::SourceType>>&
          devtools_accepted_stream_types,
      bool is_pdf,
      WeakDocumentPtr initiator_document);
  NavigationRequestInfo(const NavigationRequestInfo& other) = delete;
  ~NavigationRequestInfo();

  blink::mojom::CommonNavigationParamsPtr common_params;
  blink::mojom::BeginNavigationParamsPtr begin_params;

  // Sandbox flags inherited from the frame where this navigation occurs. In
  // particular, this does not include:
  // - Sandbox flags inherited from the creator via the PolicyContainer.
  // - Sandbox flags forced for MHTML documents.
  // - Sandbox flags from the future response via CSP.
  // It is used by the ExternalProtocolHandler to ensure sandboxed iframe won't
  // navigate the user toward a different application, which can be seen as a
  // main frame navigation somehow.
  const network::mojom::WebSandboxFlags sandbox_flags;

  // Contains information used to prevent sharing information from a navigation
  // request across first party contexts. In particular, tracks the
  // SiteForCookies, which controls what site's SameSite cookies may be set,
  // NetworkIsolationKey, which is used to restrict sharing of network
  // resources, and how to update them across redirects, which is different for
  // main frames and subresources.
  const net::IsolationInfo isolation_info;

  const bool is_primary_main_frame;

  const bool is_main_frame;

  // Whether all ancestor frames of the frame that is navigating have a secure
  // origin. True for main frames.
  const bool are_ancestors_secure;

  const int frame_tree_node_id;

  const bool report_raw_headers;

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

  // If not null, the network service will not advertise any stream types
  // (via Accept-Encoding) that are not listed. Also, it will not attempt
  // decoding any non-listed stream types.
  absl::optional<std::vector<net::SourceStream::SourceType>>
      devtools_accepted_stream_types;

  // Indicates that this navigation is for PDF content in a renderer.
  const bool is_pdf;

  // The initiator document, if still available.
  const WeakDocumentPtr initiator_document;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_INFO_H_
