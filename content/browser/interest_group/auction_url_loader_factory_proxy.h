// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// Proxy URLLoaderFactoryFactory, to limit the requests that an auction worklet
// can make.
class CONTENT_EXPORT AuctionURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  using GetUrlLoaderFactoryCallback =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  // `get_url_loader_factory_callback` must be safe to call at any time during
  // the lifetime of the AuctionURLLoaderFactoryProxy.
  //
  // URLs that may be requested are extracted from `auction_config` and
  // `bidders`. Any other requested URL will result in failure.
  AuctionURLLoaderFactoryProxy(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      GetUrlLoaderFactoryCallback get_url_loader_factory_callback,
      const url::Origin& frame_origin,
      const blink::mojom::AuctionAdConfig& auction_config,
      const std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>&
          bidders);
  AuctionURLLoaderFactoryProxy(const AuctionURLLoaderFactoryProxy&) = delete;
  AuctionURLLoaderFactoryProxy& operator=(const AuctionURLLoaderFactoryProxy&) =
      delete;
  ~AuctionURLLoaderFactoryProxy() override;

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
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;

  const GetUrlLoaderFactoryCallback get_url_loader_factory_callback_;

  const url::Origin frame_origin_;

  // URLs of worklet scripts. Requested URLs may match these URLs exactly.
  std::set<GURL> script_urls_;

  // URLs for real-time bidding data. Requests may match these with the query
  // parameter removed.
  std::set<GURL> realtime_data_urls_;

  // Expected prefix for all realtime data URL query strings.
  const std::string expected_query_prefix_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
