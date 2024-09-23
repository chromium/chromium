// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class NetworkAnonymizationKey;
}

namespace content {

// Proxy URLLoaderFactoryFactory, to limit the requests that an auction worklet
// can make.
class CONTENT_EXPORT AuctionURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  using GetUrlLoaderFactoryCallback =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  // Callback to preconnect a single socket with the given info.
  using PreconnectSocketCallback = base::OnceCallback<void(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key)>;

  using GetCookieDeprecationLabelCallback =
      base::RepeatingCallback<std::optional<std::string>()>;

  using GetDevtoolsAuctionIdsCallback =
      base::RepeatingCallback<std::vector<std::string>()>;

  // Passed in callbacks must be safe to call at any time during the lifetime of
  // the AuctionURLLoaderFactoryProxy.
  //
  // `get_frame_url_loader_factory` returns the URLLoaderFactory for the
  // associated RenderFrameHost. Only used for seller worklet scripts. Must be
  // safe to call at any point until `this` has been destroyed.
  //
  // `get_trusted_url_loader_factory` returns a trusted URLLoaderFactory. Used
  // for bidder worklet script and trusted selling signals fetches.
  //
  // `preconnect_socket_callback` is used to issue a preconnect if there's a
  // non-empty trusted signals URL. No preconnect is made for the JS and web
  // assembly, since they should be cacheable, and currently erring on the side
  // of not making unnecessary connections. Invoked immediately, if it's going
  // to be invoked at all.
  //
  // `force_reload` if true, the request will be made bypassing the cache.
  //
  // `frame_origin` is the origin of the frame running the auction. Used as the
  // initiator.
  //
  // `is_for_seller` indicates if this is for a seller or bidder workler.
  // Requests are configured differently. Seller requests use CORS and the
  // URLLoader from the renderer, while bidder requests use trusted browser
  // URLLoaderFactory, and don't use CORS, since they're same-site relative to
  // the page they were learned on.
  //
  // `client_security_state` is the ClientSecurityState to use for fetches for
  // bidder worklets. Ignored for seller worklets, since they use a
  // URLLoaderFactory with that information already attached.
  //
  // `script_url` is the Javascript URL for the worklet, `wasm_url` is a URL
  // for an optional WASM helper for the worklet, and `trusted_signals_url` is
  // the optional JSON url for additional input to the script. No other URLs may
  // be requested.
  //
  // `needs_cors_for_additional_bid` should be set if this is a bidder resource
  // that's not been verifiably selected by the bidder itself, but indirectly
  // via an additional bid, so additional checks are needed.
  AuctionURLLoaderFactoryProxy(
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
      FrameTreeNodeId frame_tree_node_id);
  AuctionURLLoaderFactoryProxy(const AuctionURLLoaderFactoryProxy&) = delete;
  AuctionURLLoaderFactoryProxy& operator=(const AuctionURLLoaderFactoryProxy&) =
      delete;
  ~AuctionURLLoaderFactoryProxy() override;

  SubresourceUrlAuthorizations& subresource_url_authorizations() {
    return subresource_url_authorizations_;
  }

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  // Returns `url` could be a valid trusted signals URL. There are two kinds of
  // trusted signals servers:
  // 1. BYOS Model(Key-Value v1):
  //   It needs to start with
  //     `<trusted_signals_base_url_>?hostname=<top_frame_origin>`.
  // 2. Server in TEE(Key-Value v2):
  //   It needs to start with `<trusted_signals_base_url_>.
  bool CouldBeTrustedSignalsUrl(const GURL& url,
                                const std::string& accept_header) const;

  mojo::PendingRemote<network::mojom::DevToolsObserver>
  CreateDevtoolsObserver();

  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;

  const GetUrlLoaderFactoryCallback get_frame_url_loader_factory_;
  const GetUrlLoaderFactoryCallback get_trusted_url_loader_factory_;
  const GetCookieDeprecationLabelCallback get_cookie_deprecation_label_;
  const GetDevtoolsAuctionIdsCallback get_devtools_auction_ids_;

  // Manages the bundle subresource URLs that may be accessed by the worklet.
  SubresourceUrlAuthorizations subresource_url_authorizations_;

  const url::Origin top_frame_origin_;
  const url::Origin frame_origin_;
  const std::optional<int> renderer_process_id_;
  const bool is_for_seller_;
  const bool force_reload_;
  const network::mojom::ClientSecurityStatePtr client_security_state_;

  // IsolationInfo used for requests using the trusted URLLoaderFactory. A
  // Transient IsolationInfo for sellers, the bidder's IsolationInfo for
  // bidders.
  const net::IsolationInfo isolation_info_;

  const FrameTreeNodeId owner_frame_tree_node_id_;
  const GURL script_url_;
  const std::optional<GURL> wasm_url_;
  const std::optional<GURL> trusted_signals_base_url_;
  const bool needs_cors_for_additional_bid_;
};

// This forwards network events from worklets to devtools.
class AuctionNetworkEventsProxy
    : public auction_worklet::mojom::AuctionNetworkEventsHandler {
 public:
  explicit AuctionNetworkEventsProxy(FrameTreeNodeId owner_frame_tree_node_id);
  AuctionNetworkEventsProxy(const AuctionURLLoaderFactoryProxy&) = delete;
  AuctionNetworkEventsProxy& operator=(const AuctionURLLoaderFactoryProxy&) =
      delete;
  ~AuctionNetworkEventsProxy() override;

  // auction_worklet::mojom::AuctionNetworkEventsHandler implementation.
  void Clone(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionNetworkEventsHandler>
          receiver) override;

  void OnNetworkSendRequest(const ::network::ResourceRequest& request,
                            ::base::TimeTicks timestamp) override;

  void OnNetworkResponseReceived(
      const std::string& request_id,
      const std::string& loader_id,
      const ::GURL& request_url,
      ::network::mojom::URLResponseHeadPtr headers) override;

  void OnNetworkRequestComplete(
      const std::string& request_id,
      const ::network::URLLoaderCompletionStatus& status) override;

 private:
  const FrameTreeNodeId owner_frame_tree_node_id_;
  mojo::ReceiverSet<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handlers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
