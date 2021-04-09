// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/public/browser/global_request_id.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/escape.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

AuctionURLLoaderFactoryProxy::AuctionURLLoaderFactoryProxy(
    scoped_refptr<network::SharedURLLoaderFactory> wrapped_factory,
    base::StringPiece publisher_hostname,
    const blink::mojom::AuctionAdConfig& auction_config,
    const std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>& bidders)
    : wrapped_factory_(std::move(wrapped_factory)),
      expected_query_prefix_(
          "hostname=" + net::EscapeQueryParamValue(publisher_hostname, true) +
          "&keys=") {
  script_urls_.insert(auction_config.decision_logic_url);
  for (const auto& bidder : bidders) {
    if (bidder->group->bidding_url)
      script_urls_.insert(*bidder->group->bidding_url);
    if (bidder->group->trusted_bidding_signals_url) {
      // Base trusted bidding signals URLs can't have query strings, since
      // running an auction will create URLs by adding query strings to them.
      DCHECK(!bidder->group->trusted_bidding_signals_url->has_query());

      realtime_data_urls_.insert(*bidder->group->trusted_bidding_signals_url);
    }
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
    return;
  }

  if (accept_header == "application/javascript") {
    // Only `script_urls_` may be requested with the Javascript Accept header.
    if (script_urls_.find(url_request.url) == script_urls_.end())
      return;
  } else if (accept_header == "application/json") {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    GURL url_without_query = url_request.url.ReplaceComponents(replacements);
    // Only `realtime_data_urls_` may be requested with the JSON Accept header.
    if (realtime_data_urls_.find(url_without_query) ==
        realtime_data_urls_.end()) {
      return;
    }

    // Make sure the query string starts with the correct prefix.
    if (!base::StartsWith(url_request.url.query_piece(),
                          expected_query_prefix_))
      return;

    // This should contain the keys value of the query string.
    base::StringPiece keys =
        url_request.url.query_piece().substr(expected_query_prefix_.size());
    // The keys value should be the last value of the query string.
    if (keys.find('&') != base::StringPiece::npos)
      return;
  } else {
    // Reject requests without accept header.
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

  // Treat this as a subresource request from the owner's origin.
  //
  // TODO(mmenke): This leaks information to the third party that made the
  // request (both the URL itself leaks information, and using the origin's NIK
  // leaks information). These leaks need to be fixed.
  new_request.trusted_params = network::ResourceRequest::TrustedParams();
  url::Origin origin = url::Origin::Create(url_request.url);
  new_request.trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies());

  // TODO(mmenke): Investigate whether `devtools_observer` or
  // `report_raw_headers` should be set when devtools is open.

  // TODO(mmenke): Investigate whether `client_security_state` should be
  // populated.

  wrapped_factory_->CreateLoaderAndStart(
      std::move(receiver),
      // These are browser-initiated requests, so give them a browser request
      // ID. Extension APIs may expect these to be unique.
      GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, new_request, std::move(client),
      traffic_annotation);
}

void AuctionURLLoaderFactoryProxy::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  // Not currently needed.
  NOTREACHED();
}

}  // namespace content
