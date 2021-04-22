// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_downloader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace auction_worklet {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("auction_downloader", R"(
        semantics {
          sender: "AuctionDownloader"
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

// Returns the MIME type string to send for the Accept header for `mime_type`.
// These are the official IANA MIME type strings, though other MIME type strings
// are allows in the response.
base::StringPiece MimeTypeToString(AuctionDownloader::MimeType mime_type) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kJavascript:
      return base::StringPiece("application/javascript");
    case AuctionDownloader::MimeType::kJson:
      return base::StringPiece("application/json");
  }
}

// Checks if `advertised_mime_type` is consistent with `mime_type`.
// `advertised_mime_type` must be lowercase.
bool MimeTypeIsConsistent(AuctionDownloader::MimeType mime_type,
                          const std::string& advertised_mime_type) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kJavascript:
      return blink::IsSupportedJavascriptMimeType(advertised_mime_type);
    case AuctionDownloader::MimeType::kJson:
      return blink::IsJSONMimeType(advertised_mime_type);
  }
}

// Checks if `charset` is a valid charset, in lowercase ASCII. Takes `body` as
// well, to ensure it uses the specified charset.
bool IsAllowedCharset(base::StringPiece charset, const std::string& body) {
  if (charset == "utf-8" || charset.empty()) {
    return base::IsStringUTF8(body);
  } else if (charset == "us-ascii") {
    return base::IsStringASCII(body);
  }
  // TODO(mmenke): Worth supporting iso-8859-1, or full character set list?
  return false;
}

}  // namespace

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    MimeType mime_type,
    AuctionDownloaderCallback auction_downloader_callback)
    : mime_type_(mime_type),
      auction_downloader_callback_(std::move(auction_downloader_callback)) {
  DCHECK(auction_downloader_callback_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = source_url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      MimeTypeToString(mime_type_));

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

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

  auto simple_url_loader = std::move(simple_url_loader_);
  std::string allow_fledge;
  if (!body || !simple_url_loader->ResponseInfo()->headers ||
      !simple_url_loader->ResponseInfo()->headers->GetNormalizedHeader(
          "X-Allow-FLEDGE", &allow_fledge) ||
      !base::EqualsCaseInsensitiveASCII(allow_fledge, "true") ||
      // Note that ResponseInfo's `mime_type` is always lowercase.
      !MimeTypeIsConsistent(mime_type_,
                            simple_url_loader->ResponseInfo()->mime_type) ||
      !IsAllowedCharset(simple_url_loader->ResponseInfo()->charset, *body)) {
    std::move(auction_downloader_callback_).Run(nullptr);
    return;
  }

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
