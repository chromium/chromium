// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// Proxy URLLoaderFactoryFactory, to limit the requests that an auction worklet
// can make.
class CONTENT_EXPORT AuctionURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  // URLs that may be requested are extracted from `auction_config` and
  // `bidders`. Any other requested URL will result in failure.
  AuctionURLLoaderFactoryProxy(
      scoped_refptr<network::SharedURLLoaderFactory> wrapped_factory,
      base::StringPiece publisher_hostname,
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
  scoped_refptr<network::SharedURLLoaderFactory> wrapped_factory_;

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
