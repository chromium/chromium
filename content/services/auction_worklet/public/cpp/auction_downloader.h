// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "net/base/network_interfaces.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace auction_worklet {

// Download utility for auction scripts and JSON data. Creates requests and
// blocks responses.
class CONTENT_EXPORT AuctionDownloader {
 public:
  // Mime type to use for Accept header. Any response without a matching
  // Content-Type header is rejected.
  enum class MimeType {
    kAdAuctionTrustedSignals,
    kJavascript,
    kJson,
    kWebAssembly,
  };

  // This determines how the downloaded data is handled.
  enum class DownloadMode {
    // The data is collected in memory and passed to the callback.
    kActualDownload,

    // The data is discarded as it comes in and the callback is invoked with an
    // empty string.
    kSimulatedDownload
  };

  using ResponseStartedCallback =
      base::OnceCallback<void(const network::mojom::URLResponseHead&)>;

  // Passes in nullptr on failure. Always invoked asynchronously. Will not be
  // invoked after the AuctionDownloader is destroyed.
  using AuctionDownloaderCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              scoped_refptr<net::HttpResponseHeaders> headers,
                              std::optional<std::string> error)>;

  // When a URL appears in a network error message, it's truncated to never be
  // longed than this length.
  static constexpr size_t kMaxErrorUrlLength = 10 * 1024;

  // This handles how network requests get logged to devtools.
  class CONTENT_EXPORT NetworkEventsDelegate {
   public:
    NetworkEventsDelegate() = default;
    virtual ~NetworkEventsDelegate();

    virtual void OnNetworkSendRequest(network::ResourceRequest& request) = 0;
    virtual void OnNetworkResponseReceived(
        const GURL& url,
        const network::mojom::URLResponseHead& head) = 0;

    virtual void OnNetworkRequestComplete(
        const network::URLLoaderCompletionStatus& status) = 0;
  };

  // Starts loading `source_url` on construction.
  //
  // The post_body and content_type fields are optional and used for POST
  // requests. While the content_type header is not mandatory for POST requests,
  // it should be set to indicate the content type of the request.
  //
  // `response_started_callback` is optional, and will be invoked once the
  // response headers have been received if they are for a 2xx with an
  // appropriate Ad-Auction-Allowed header.
  //
  // `auction_downloader_callback` will be invoked asynchronously once the data
  // has been fetched or an error has occurred.
  //
  // When `response_started_callback` is set, the following sequences of
  // callback invocations are possible:
  //
  // 1) `auction_downloader_callback` with a failure. This happens e.g.
  //    if the response doesn't have the proper Ad-Auction-Allowed header.
  //
  // 2) `response_started_callback` followed by an `auction_downloader_callback`
  //    with a failure. This means the headers were received fine and checked
  //    out, but something went wrong afterwards. This also includes the
  //    mimetype and charset check.
  //
  // 3) `response_started_callback` followed by an `auction_downloader_callback`
  //    with a success.
  AuctionDownloader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const GURL& source_url,
      DownloadMode download_mode,
      MimeType mime_type,
      std::optional<std::string> post_body,
      std::optional<std::string> content_type,
      ResponseStartedCallback response_started_callback,
      AuctionDownloaderCallback auction_downloader_callback,
      std::unique_ptr<NetworkEventsDelegate> network_events_delegate);
  explicit AuctionDownloader(const AuctionDownloader&) = delete;
  AuctionDownloader& operator=(const AuctionDownloader&) = delete;
  ~AuctionDownloader();

  const GURL& source_url() const { return source_url_; }

 private:
  void OnHeadersOnlyReceived(scoped_refptr<net::HttpResponseHeaders> headers);

  void OnBodyReceived(std::unique_ptr<std::string> body);

  void OnRedirect(const GURL& url_before_redirect,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Notifies tracing, devtools and callback of a failure and cancels any
  // further loading.
  void FailRequest(network::URLLoaderCompletionStatus status,
                   std::string error_string);
  void TraceResult(bool failure,
                   base::TimeTicks completion_time,
                   int64_t encoded_data_length,
                   int64_t decoded_body_length);

  const GURL source_url_;
  const MimeType mime_type_;
  // A UnguessableToken string to be used in devtools.
  std::string request_id_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  ResponseStartedCallback response_started_callback_;
  AuctionDownloaderCallback auction_downloader_callback_;
  std::unique_ptr<NetworkEventsDelegate> network_events_delegate_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_
