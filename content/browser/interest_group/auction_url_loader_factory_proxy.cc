// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/global_request_id.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
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
    GetCookieDeprecationLabelCallback get_cookie_deprecation_label,
    GetDevtoolsAuctionIdsCallback get_devtools_auction_ids,
    bool force_reload,
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    std::optional<int> renderer_process_id,
    bool is_for_seller,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const GURL& script_url,
    const std::optional<GURL>& wasm_url,
    const std::optional<GURL>& trusted_signals_base_url,
    bool needs_cors_for_additional_bid,
    FrameTreeNodeId frame_tree_node_id)
    : receiver_(this, std::move(pending_receiver)),
      get_frame_url_loader_factory_(std::move(get_frame_url_loader_factory)),
      get_trusted_url_loader_factory_(
          std::move(get_trusted_url_loader_factory)),
      get_cookie_deprecation_label_(std::move(get_cookie_deprecation_label)),
      get_devtools_auction_ids_(std::move(get_devtools_auction_ids)),
      top_frame_origin_(top_frame_origin),
      frame_origin_(frame_origin),
      renderer_process_id_(renderer_process_id),
      is_for_seller_(is_for_seller),
      force_reload_(force_reload),
      client_security_state_(std::move(client_security_state)),
      isolation_info_(is_for_seller ? net::IsolationInfo::CreateTransient()
                                    : CreateBidderIsolationInfo(
                                          url::Origin::Create(script_url))),
      owner_frame_tree_node_id_(frame_tree_node_id),
      script_url_(script_url),
      wasm_url_(wasm_url),
      trusted_signals_base_url_(trusted_signals_base_url),
      needs_cors_for_additional_bid_(needs_cors_for_additional_bid) {
  DCHECK(client_security_state_);
  if (trusted_signals_base_url_) {
    std::move(preconnect_socket_callback)
        .Run(*trusted_signals_base_url_,
             isolation_info_.network_anonymization_key());
  }

  // `needs_cors_for_additional_bid_` applies only to buyer stuff.
  DCHECK(!(is_for_seller_ && needs_cors_for_additional_bid_));
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
  std::optional<std::string> opt_accept_header =
      url_request.headers.GetHeader(net::HttpRequestHeaders::kAccept);
  if (!opt_accept_header) {
    receiver_.ReportBadMessage("Missing accept header");
    return;
  }
  std::string accept_header = std::move(opt_accept_header).value();

  bool is_request_allowed = false;
  bool is_trusted_signals_request = false;
  std::optional<InterestGroupAuctionFetchType> event_type;

  const SubresourceUrlBuilder::BundleSubresourceInfo* maybe_subresource_info =
      nullptr;
  std::optional<network::ResourceRequest::WebBundleTokenParams>
      maybe_web_bundle_token_params;
  if (url_request.url == script_url_ &&
      accept_header == "application/javascript") {
    is_request_allowed = true;
    event_type = is_for_seller_ ? InterestGroupAuctionFetchType::kSellerJs
                                : InterestGroupAuctionFetchType::kBidderJs;
  } else if (wasm_url_.has_value() && url_request.url == wasm_url_.value() &&
             accept_header == "application/wasm") {
    event_type = InterestGroupAuctionFetchType::kBidderWasm;
    is_request_allowed = true;
  } else if (CouldBeTrustedSignalsUrl(url_request.url, accept_header)) {
    event_type = is_for_seller_
                     ? InterestGroupAuctionFetchType::kSellerTrustedSignals
                     : InterestGroupAuctionFetchType::kBidderTrustedSignals;
    is_request_allowed = true;
    is_trusted_signals_request = true;
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
    // Debugging for https://crbug.com/1448458
    SCOPED_CRASH_KEY_STRING32("fledge", "req-accept", accept_header);
    SCOPED_CRASH_KEY_STRING256("fledge", "req-url",
                               url_request.url.possibly_invalid_spec());
    SCOPED_CRASH_KEY_STRING256("fledge", "expect-script-url",
                               script_url_.possibly_invalid_spec());
    SCOPED_CRASH_KEY_STRING256(
        "fledge", "expect-wasm-url",
        wasm_url_.value_or(GURL()).possibly_invalid_spec());
    SCOPED_CRASH_KEY_STRING256(
        "fledge", "expect-trusted",
        trusted_signals_base_url_.value_or(GURL()).possibly_invalid_spec());
    SCOPED_CRASH_KEY_STRING256("fledge", "expect-top-frame",
                               top_frame_origin_.host());
    receiver_.ReportBadMessage("Unexpected request");
    return;
  }

  bool is_cross_origin_enabled_trusted_signals_request = false;
  if (is_trusted_signals_request &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    is_cross_origin_enabled_trusted_signals_request = true;
  }

  // Create fresh request object, only keeping the URL field and Accept request
  // header for GET requests, to protect against compromised auction worklet
  // processes setting values that should not have access to (e.g., sending
  // credentialed requests). Only the URL and traffic annotation of the original
  // request are used.
  // For POST requests, also move over request method, body and content-type.
  network::ResourceRequest new_request;
  new_request.url = url_request.url;
  new_request.web_bundle_token_params =
      std::move(maybe_web_bundle_token_params);
  new_request.devtools_request_id = url_request.devtools_request_id;
  new_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                accept_header);
  new_request.redirect_mode = network::mojom::RedirectMode::kError;
  new_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  new_request.request_initiator = frame_origin_;
  new_request.enable_load_timing = url_request.enable_load_timing;

  if (url_request.method == net::HttpRequestHeaders::kPostMethod) {
    new_request.method = std::move(url_request.method);
    new_request.request_body = std::move(url_request.request_body);
    std::optional<std::string> content_type =
        url_request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
    if (content_type.has_value()) {
      new_request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                    std::move(content_type).value());
    }
  }

  if (event_type.has_value() && new_request.devtools_request_id.has_value() &&
      devtools_instrumentation::NeedInterestGroupAuctionEvents(
          owner_frame_tree_node_id_)) {
    std::vector<std::string> relevant_auction_ids =
        get_devtools_auction_ids_.Run();
    devtools_instrumentation::OnInterestGroupAuctionNetworkRequestCreated(
        owner_frame_tree_node_id_, *event_type,
        *new_request.devtools_request_id, relevant_auction_ids);
  }

  if (is_trusted_signals_request) {
    std::optional<std::string> maybe_deprecation_label =
        get_cookie_deprecation_label_.Run();
    if (maybe_deprecation_label) {
      new_request.headers.SetHeader("Sec-Cookie-Deprecation",
                                    *maybe_deprecation_label);
    }
  }

  if (is_cross_origin_enabled_trusted_signals_request) {
    // For cross-origin trusted signals request, the principal is the origin
    // of the script.
    new_request.request_initiator = url::Origin::Create(script_url_);
  }

  if (force_reload_) {
    new_request.load_flags = net::LOAD_BYPASS_CACHE;
  }

  if (maybe_subresource_info || needs_cors_for_additional_bid_ ||
      is_cross_origin_enabled_trusted_signals_request) {
    // CORS is needed.
    //
    // For subresource bundle requests, CORS is supported if the subresource
    // URL's scheme is https and not uuid-in-package. However, unlike
    // traditional network requests, the browser cannot read the response if
    // kNoCors is used, even with CORS-safe methods and headers -- the response
    // is blocked by ORB.
    new_request.mode = network::mojom::RequestMode::kCors;
  } else {
    // CORS is not needed.
    //
    // For bidder worklets, the requests are same origin to the InterestGroup's
    // owner, which was either added by the owner itself, or by a third party
    // explicitly allowed to do so.
    //
    // For seller worklets, while the publisher page provides both the script
    // and the trusted signals URLs, both requests use safe methods (GET), and
    // don't set any headers, so CORS is not needed. ORB would block the
    // signal's JSON response, if made in the context of the page, but the JSON
    // is only made available to the same-origin script, so ORB isn't needed
    // here.
    //
    // This does not apply if we permit trusted signals to be cross-origin from
    // the corresponding script, in which has the signals origin's permission is
    // required before sharing its data with the script.
    new_request.mode = network::mojom::RequestMode::kNoCors;
  }

  GetUrlLoaderFactoryCallback url_loader_factory_getter =
      get_trusted_url_loader_factory_;
  if (is_for_seller_) {
    if (!is_trusted_signals_request && !maybe_subresource_info) {
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
  } else if (needs_cors_for_additional_bid_) {
    // For additional bid reporting, act like the frame provided it as well.
    url_loader_factory_getter = get_frame_url_loader_factory_;
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

  bool network_instrumentation_enabled = false;
  if (owner_frame_tree_node_id_) {
    FrameTreeNode* owner_frame_tree_node =
        FrameTreeNode::GloballyFindByID(owner_frame_tree_node_id_);
    new_request.throttling_profile_id =
        owner_frame_tree_node->current_frame_host()->devtools_frame_token();

    devtools_instrumentation::ApplyAuctionNetworkRequestOverrides(
        owner_frame_tree_node, &new_request, &network_instrumentation_enabled);
  }

  if (network_instrumentation_enabled) {
    new_request.enable_load_timing = true;
    if (new_request.trusted_params.has_value()) {
      new_request.trusted_params->devtools_observer = CreateDevtoolsObserver();
    }
  }

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
  NOTREACHED_IN_MIGRATION();
}

AuctionNetworkEventsProxy::AuctionNetworkEventsProxy(
    FrameTreeNodeId owner_frame_tree_node_id)
    : owner_frame_tree_node_id_(owner_frame_tree_node_id) {}

AuctionNetworkEventsProxy::~AuctionNetworkEventsProxy() = default;

void AuctionNetworkEventsProxy::Clone(
    mojo::PendingReceiver<auction_worklet::mojom::AuctionNetworkEventsHandler>
        receiver) {
  if (receiver.is_valid()) {
    auction_network_events_handlers_.Add(this, std::move(receiver));
  }
}

void AuctionNetworkEventsProxy::OnNetworkSendRequest(
    const ::network::ResourceRequest& request,
    ::base::TimeTicks timestamp) {
  devtools_instrumentation::OnAuctionWorkletNetworkRequestWillBeSent(
      owner_frame_tree_node_id_, request, timestamp);
}
void AuctionNetworkEventsProxy::OnNetworkResponseReceived(
    const std::string& request_id,
    const std::string& loader_id,
    const ::GURL& request_url,
    ::network::mojom::URLResponseHeadPtr headers) {
  devtools_instrumentation::OnAuctionWorkletNetworkResponseReceived(
      owner_frame_tree_node_id_, request_id, loader_id, request_url, *headers);
}
void AuctionNetworkEventsProxy::OnNetworkRequestComplete(
    const std::string& request_id,
    const ::network::URLLoaderCompletionStatus& status) {
  devtools_instrumentation::OnAuctionWorkletNetworkRequestComplete(
      owner_frame_tree_node_id_, request_id, status);
}

bool AuctionURLLoaderFactoryProxy::CouldBeTrustedSignalsUrl(
    const GURL& url,
    const std::string& accept_header) const {
  if (!trusted_signals_base_url_) {
    return false;
  }

  if (accept_header != "application/json" &&
      accept_header != "message/ad-auction-trusted-signals-response") {
    return false;
  }

  // Simplest way to make sure the requested URL exactly matches
  // `trusted_signals_base_url_` (which has no query or reference component),
  // except for an added query component is to check there's no reference, and
  // then make sure it starts with the `trusted_signals_base_url_`, with a
  // partial query component appended. Seems not worth fully disecting the query
  // string to make sure its only keys are hostname, keys, renderUrls, and
  // adComponentsRenderUrls.
  if (url.has_ref()) {
    return false;
  }

  // GURL's Mojo serialization logic may convert a valid URL that's too long
  // to an invalid one. Since trusted signals URLs may be appended to, an
  // invalid URL may have begun life as a valid trusted signals URL, but then
  // exceeded the max length when appended to. The request for such a URL will
  // fail, but that's how we normally treat such URLs, so just let it through.
  if (!url.is_valid()) {
    return true;
  }

  if (accept_header == "application/json") {
    std::string full_prefix = base::StringPrintf(
        "%s?hostname=%s&", trusted_signals_base_url_->spec().c_str(),
        top_frame_origin_.host().c_str());
    return base::StartsWith(url.spec(), full_prefix,
                            base::CompareCase::SENSITIVE);
  } else {
    return url.spec() == trusted_signals_base_url_->spec();
  }
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
AuctionURLLoaderFactoryProxy::CreateDevtoolsObserver() {
  if (owner_frame_tree_node_id_) {
    FrameTreeNode* initiator_frame_tree_node =
        FrameTreeNode::GloballyFindByID(owner_frame_tree_node_id_);

    if (initiator_frame_tree_node) {
      return NetworkServiceDevToolsObserver::MakeSelfOwned(
          initiator_frame_tree_node);
    }
  }
  return mojo::PendingRemote<network::mojom::DevToolsObserver>();
}

}  // namespace content
