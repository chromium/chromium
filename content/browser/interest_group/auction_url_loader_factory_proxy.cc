// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Helper to create the IsolationInfo used for all bidder requests. A helper
// method is used to avoid having to construct two copies of `bidder_origin`.
net::IsolationInfo CreateBidderIsolationInfo(const url::Origin& bidder_origin) {
  return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                    bidder_origin, bidder_origin,
                                    net::SiteForCookies());
}

}  // namespace

AuctionURLLoaderFactoryProxy::AuctionURLLoaderFactoryProxy(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    GetUrlLoaderFactoryCallback get_frame_url_loader_factory,
    GetUrlLoaderFactoryCallback get_trusted_url_loader_factory,
    PreconnectSocketCallback preconnect_socket_callback,
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    absl::optional<int> renderer_process_id,
    bool is_for_seller,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const GURL& script_url,
    const absl::optional<GURL>& wasm_url,
    const absl::optional<GURL>& trusted_signals_base_url)
    : receiver_(this, std::move(pending_receiver)),
      get_frame_url_loader_factory_(std::move(get_frame_url_loader_factory)),
      get_trusted_url_loader_factory_(
          std::move(get_trusted_url_loader_factory)),
      top_frame_origin_(top_frame_origin),
      frame_origin_(frame_origin),
      renderer_process_id_(renderer_process_id),
      is_for_seller_(is_for_seller),
      client_security_state_(std::move(client_security_state)),
      isolation_info_(is_for_seller ? net::IsolationInfo::CreateTransient()
                                    : CreateBidderIsolationInfo(
                                          url::Origin::Create(script_url))),
      script_url_(script_url),
      wasm_url_(wasm_url),
      trusted_signals_base_url_(trusted_signals_base_url) {
  DCHECK(client_security_state_);
  if (trusted_signals_base_url_) {
    std::move(preconnect_socket_callback)
        .Run(*trusted_signals_base_url_,
             isolation_info_.network_anonymization_key());
  }
}

AuctionURLLoaderFactoryProxy::~AuctionURLLoaderFactoryProxy() = default;

void AuctionURLLoaderFactoryProxy::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Worklet requests must include a request header.
  std::string accept_header;
  if (!url_request.headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                     &accept_header)) {
    receiver_.ReportBadMessage("Missing accept header");
    return;
  }

  bool is_request_allowed = false;
  bool is_trusted_bidding_signals_request = false;

  const SubresourceUrlBuilder::BundleSubresourceInfo* maybe_subresource_info =
      nullptr;
  absl::optional<network::ResourceRequest::WebBundleTokenParams>
      maybe_web_bundle_token_params;
  if (url_request.url == script_url_ &&
      accept_header == "application/javascript") {
    is_request_allowed = true;
  } else if (wasm_url_.has_value() && url_request.url == wasm_url_.value() &&
             accept_header == "application/wasm") {
    is_request_allowed = true;
  } else if (CouldBeTrustedSignalsUrl(url_request.url) &&
             accept_header == "application/json") {
    is_request_allowed = true;
    is_trusted_bidding_signals_request = true;
  } else {
    maybe_subresource_info =
        subresource_url_authorizations_.GetAuthorizationInfo(url_request.url);
    if (maybe_subresource_info != nullptr) {
      DCHECK(renderer_process_id_);
      is_request_allowed = true;
      maybe_web_bundle_token_params =
          network::ResourceRequest::WebBundleTokenParams(
              /*bundle_url=*/
              maybe_subresource_info->info_from_renderer.bundle_url,
              /*token=*/maybe_subresource_info->info_from_renderer.token,
              /*render_process_id=*/*renderer_process_id_);
    }
  }

  if (!is_request_allowed) {
    receiver_.ReportBadMessage("Unexpected request");
    return;
  }

  // Create fresh request object, only keeping the URL field and Accept request
  // header, to protect against compromised auction worklet processes setting
  // values that should not have access to (e.g., sending credentialed
  // requests). Only the URL and traffic annotation of the original request are
  // used.
  network::ResourceRequest new_request;
  new_request.url = url_request.url;
  new_request.web_bundle_token_params =
      std::move(maybe_web_bundle_token_params);
  new_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                accept_header);
  new_request.redirect_mode = network::mojom::RedirectMode::kError;
  new_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  new_request.request_initiator = frame_origin_;
  new_request.enable_load_timing = url_request.enable_load_timing;

  if (!maybe_subresource_info) {
    // CORS is not needed.
    //
    // For bidder worklets, the requests are same origin to the InterestGroup's
    // owner, which was either added by the owner itself, or by a third party
    // explicitly allowed to do so.
    //
    // For seller worklets, while the publisher page provides both the script
    // and the trusted signals URLs, both requests use safe methods (GET), and
    // don't set any headers, so CORS is not needed. CORB would block the
    // signal's JSON response, if made in the context of the page, but the JSON
    // is only made available to the same-origin script, so CORB isn't needed
    // here.
    new_request.mode = network::mojom::RequestMode::kNoCors;
  } else {
    // CORS is needed.
    //
    // For subresource bundle requests, CORS is supported if the subresource
    // URL's scheme is https and not uuid-in-package. However, unlike
    // traditional network requests, the browser cannot read the response if
    // kNoCors is used, even with CORS-safe methods and headers -- the response
    // is blocked by CORB.
    new_request.mode = network::mojom::RequestMode::kCors;
  }

  GetUrlLoaderFactoryCallback url_loader_factory_getter =
      get_trusted_url_loader_factory_;
  if (is_for_seller_) {
    if (!is_trusted_bidding_signals_request && !maybe_subresource_info) {
      // The script URL is provided in its entirety by the frame initiating the
      // auction, so just use its URLLoaderFactory for those requests.
      url_loader_factory_getter = get_frame_url_loader_factory_;
    } else {
      // Other URLs combine a base URL from the frame's Javascript and data from
      // bidding interest groups, so use a (single) transient IsolationInfo for
      // them, to prevent exposing data across sites.
      //
      // The exception is DirectFromSellerSignals -- these don't come from
      // interest groups, and should use the page's isolation info. However,
      // they cannot use the page's URLLoaderFactory, since `trusted_params`
      // needs to be populated to bypass the X-FLEDGE-Auction-Only request
      // block.
      new_request.trusted_params = network::ResourceRequest::TrustedParams();
      if (maybe_subresource_info) {
        url::Origin origin = url::Origin::Create(url_request.url);
        new_request.trusted_params->isolation_info =
            net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                       origin, origin, net::SiteForCookies());
      } else {
        new_request.trusted_params->isolation_info = isolation_info_;
      }
      new_request.trusted_params->client_security_state =
          client_security_state_.Clone();
    }
  } else {
    // Treat this as a subresource request from the owner's origin, using the
    // trusted URLLoaderFactory.
    //
    // TODO(mmenke): This leaks information to the third party that made the
    // request (both the URL itself leaks information, and using the origin's
    // NIK leaks information). These leaks need to be fixed.
    new_request.trusted_params = network::ResourceRequest::TrustedParams();
    new_request.trusted_params->isolation_info = isolation_info_;
    new_request.trusted_params->client_security_state =
        client_security_state_.Clone();
  }

  // TODO(mmenke): Investigate whether `devtools_observer` or
  // `report_raw_headers` should be set when devtools is open.

  url_loader_factory_getter.Run()->CreateLoaderAndStart(
      std::move(receiver),
      // These are browser-initiated requests, so give them a browser request
      // ID. Extension APIs may expect these to be unique.
      GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, new_request, std::move(client),
      traffic_annotation);
}

void AuctionURLLoaderFactoryProxy::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

bool AuctionURLLoaderFactoryProxy::CouldBeTrustedSignalsUrl(
    const GURL& url) const {
  if (!trusted_signals_base_url_)
    return false;

  // Simplest way to make sure the requested URL exactly matches
  // `trusted_signals_base_url_` (which has no query or reference component),
  // except for an added query component is to check there's no reference, and
  // then make sure it starts with the `trusted_signals_base_url_`, with a
  // partial query component appended. Seems not worth fully disecting the query
  // string to make sure its only keys are hostname, keys, renderUrls, and
  // adComponentsRenderUrls.
  if (url.has_ref())
    return false;
  std::string full_prefix = base::StringPrintf(
      "%s?hostname=%s&", trusted_signals_base_url_->spec().c_str(),
      top_frame_origin_.host().c_str());
  return base::StartsWith(url.spec(), full_prefix,
                          base::CompareCase::SENSITIVE);
}

}  // namespace content
