// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_downloader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace auction_worklet {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("hintsfetcher_gethintsrequest", R"(
        semantics {
          sender: "BidderWorklet"
          description:
            "Requests FLEDGE script or JSON file for running an ad auction."
          trigger:
            "Requested when running a FLEDGE auction."
          data: "URL associated with an interest group or seller."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests cannot be disabled."
          policy_exception_justification:
            "These requests triggered by a website."
        })");

}  // namespace

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    AuctionDownloaderCallback auction_downloader_callback)
    : auction_downloader_callback_(std::move(auction_downloader_callback)) {
  DCHECK(auction_downloader_callback_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = source_url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  // TODO(mmenke): Set MIME type / reject unexpected MIME types.
  // TODO(mmenke): Reject unexpected charsets.

  // Abort on redirects.
  // TODO(mmenke): May want a browser-side proxy to block redirects instead.
  simple_url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &AuctionDownloader::OnRedirect, base::Unretained(this)));

  // TODO(mmenke): Consider limiting the size of response bodies.
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, base::BindOnce(&AuctionDownloader::OnBodyReceived,
                                         base::Unretained(this)));
}

AuctionDownloader::~AuctionDownloader() = default;

void AuctionDownloader::OnBodyReceived(std::unique_ptr<std::string> body) {
  DCHECK(auction_downloader_callback_);

  simple_url_loader_.reset();
  std::move(auction_downloader_callback_).Run(std::move(body));
}

void AuctionDownloader::OnRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(auction_downloader_callback_);

  // Need to cancel the load, to prevent the request from continuing.
  simple_url_loader_.reset();

  std::move(auction_downloader_callback_).Run(nullptr /* body */);
}

}  // namespace auction_worklet
