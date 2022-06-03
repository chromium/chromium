// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/escape.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

AuctionURLLoaderFactoryProxy::AuctionURLLoaderFactoryProxy(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    GetUrlLoaderFactoryCallback get_frame_url_loader_factory,
    GetUrlLoaderFactoryCallback get_trusted_url_loader_factory,
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    bool is_for_seller,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const GURL& script_url,
    const absl::optional<GURL>& trusted_signals_base_url)
    : receiver_(this, std::move(pending_receiver)),
      get_frame_url_loader_factory_(std::move(get_frame_url_loader_factory)),
      get_trusted_url_loader_factory_(
          std::move(get_trusted_url_loader_factory)),
      top_frame_origin_(top_frame_origin),
      frame_origin_(frame_origin),
      is_for_seller_(is_for_seller),
      client_security_state_(std::move(client_security_state)),
      script_url_(script_url),
      trusted_signals_base_url_(trusted_signals_base_url) {
  DCHECK(client_security_state_);
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

  if (url_request.url == script_url_ &&
      accept_header == "application/javascript") {
    is_request_allowed = true;
  } else if (CouldBeTrustedSignalsUrl(url_request.url) &&
             accept_header == "application/json") {
    is_request_allowed = true;
    is_trusted_bidding_signals_request = true;
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
  new_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                accept_header);
  new_request.redirect_mode = network::mojom::RedirectMode::kError;
  new_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  new_request.request_initiator = frame_origin_;

  GetUrlLoaderFactoryCallback url_loader_factory_getter =
      get_trusted_url_loader_factory_;
  if (is_for_seller_) {
    // Seller requests are made to base URLs specified by the publisher page,
    // These URLs are generally cross-origin to the publisher, so need to always
    // use CORS.
    new_request.mode = network::mojom::RequestMode::kCors;

    if (!is_trusted_bidding_signals_request) {
      // The script URL is provided in its entirety by the frame initiating the
      // auction, so just use its URLLoaderFactory for those requests.
      url_loader_factory_getter = get_frame_url_loader_factory_;
    } else {
      // Other URLs combine a base URL from the frame's Javascript and data from
      // bidding interest groups, so use a (single) transient IsolationInfo for
      // them, to prevent exposing data across sites.
      if (isolation_info_for_seller_signals_.IsEmpty()) {
        isolation_info_for_seller_signals_ =
            net::IsolationInfo::CreateTransient();
      }

      new_request.trusted_params = network::ResourceRequest::TrustedParams();
      new_request.trusted_params->isolation_info =
          isolation_info_for_seller_signals_;
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
    new_request.mode = network::mojom::RequestMode::kNoCors;
    new_request.trusted_params = network::ResourceRequest::TrustedParams();
    url::Origin origin = url::Origin::Create(url_request.url);
    new_request.trusted_params->isolation_info =
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   origin, origin, net::SiteForCookies());
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
  std::string full_prefix = base::StringPrintf(
      "%s?hostname=%s&keys=", trusted_signals_base_url_->spec().c_str(),
      top_frame_origin_.host().c_str());
  if (!base::StartsWith(url.spec(), full_prefix, base::CompareCase::SENSITIVE))
    return false;
  return url.spec().find_first_of("&#=", full_prefix.size()) ==
         std::string::npos;
}

}  // namespace content
