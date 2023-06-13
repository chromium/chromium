// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  // Passes in nullptr on failure. Always invoked asynchronously. Will not be
  // invoked after the AuctionDownloader is destroyed.
  using AuctionDownloaderCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              scoped_refptr<net::HttpResponseHeaders> headers,
                              absl::optional<std::string> error)>;

  // Starts loading `source_url` on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  AuctionDownloader(network::mojom::URLLoaderFactory* url_loader_factory,
                    const GURL& source_url,
                    DownloadMode download_mode,
                    MimeType mime_type,
                    AuctionDownloaderCallback auction_downloader_callback);
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
  std::string GetRequestId();
  void TraceResult(bool failure,
                   base::TimeTicks completion_time,
                   int64_t encoded_data_length,
                   int64_t decoded_body_length);

  const GURL source_url_;
  const MimeType mime_type_;
  // Filled in lazily if tracing is actually used.
  absl::optional<base::UnguessableToken> request_id_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  AuctionDownloaderCallback auction_downloader_callback_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_
