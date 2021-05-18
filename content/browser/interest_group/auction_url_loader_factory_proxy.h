// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {

// Proxy URLLoaderFactoryFactory, to limit the requests that an auction worklet
// can make.
class CONTENT_EXPORT AuctionURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  using GetUrlLoaderFactoryCallback =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  // Passed in callbacks must be safe to call at any time during the lifetime of
  // the AuctionURLLoaderFactoryProxy.
  //
  // `get_url_loader_factory` returns the URLLoaderFactory to use. Must be safe
  // to call at any point until `this` has been destroyed.
  //
  // `frame_origin` is the origin of the frame running the auction. Used as the
  // initiator.
  //
  // `use_cors` indicates if requests should use CORS or not. Should be true for
  // seller worklets.
  //
  // `script_url` is the Javascript URL for the worklet, and
  // `trusted_signals_url` is the optional JSON url for additional input to the
  // script. No other URLs may be requested.
  AuctionURLLoaderFactoryProxy(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      GetUrlLoaderFactoryCallback get_url_loader_factory,
      const url::Origin& frame_origin,
      bool use_cors,
      const GURL& script_url,
      const absl::optional<GURL>& trusted_signals_url = absl::nullopt);
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

  const GetUrlLoaderFactoryCallback get_url_loader_factory_;

  const url::Origin frame_origin_;
  const bool use_cors_;

  const GURL script_url_;
  const absl::optional<GURL> trusted_signals_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
