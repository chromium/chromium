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
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/in_progress_auction_download.mojom-forward.h"
#include "net/base/network_interfaces.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

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

  // Timeout for network requests it makes.
  static constexpr base::TimeDelta kRequestTimeout = base::Seconds(30);

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
      std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
      ResponseStartedCallback response_started_callback,
      AuctionDownloaderCallback auction_downloader_callback,
      std::unique_ptr<NetworkEventsDelegate> network_events_delegate);

  // Alternative constructor that allows adopting an in-progress request via an
  // InProgressAuctionDownloadPtr, which should have been produced via
  // AuctionDownloader::StartDownload. The URLLoaderFactory will only be used if
  // async revalidation is required.
  AuctionDownloader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojom::InProgressAuctionDownloadPtr in_progress_load,
      DownloadMode download_mode,
      MimeType mime_type,
      std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
      ResponseStartedCallback response_started_callback,
      AuctionDownloaderCallback auction_downloader_callback,
      std::unique_ptr<NetworkEventsDelegate> network_events_delegate);

  // Alternative constructor, for use when used in the browser process directly,
  // rather than in conjunction with a AuctionURLLoaderFactoryProxy. Takes an
  // initiator and ResourceRequest::TrustedParams. Creation of TrustedParams
  // from an IPAddressSpace requires content/browser code, so this method can't
  // take an IPAddressSpace and IsolationInfo and construct it from them.
  AuctionDownloader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const GURL& source_url,
      DownloadMode download_mode,
      MimeType mime_type,
      std::optional<std::string> post_body,
      std::optional<std::string> content_type,
      const url::Origin& request_initiator,
      network::ResourceRequest::TrustedParams trusted_params,
      AuctionDownloaderCallback auction_downloader_callback,
      std::unique_ptr<NetworkEventsDelegate> network_events_delegate);

  explicit AuctionDownloader(const AuctionDownloader&) = delete;
  AuctionDownloader& operator=(const AuctionDownloader&) = delete;
  ~AuctionDownloader();

  const GURL& source_url() const { return source_url_; }

  const std::string& request_id() const { return request_id_; }

  // Start a download to be used later with the constructor that takes an
  // InProgressAuctionDownloadPtr. The request will be canceled if the
  // InProgressAuctionDownloadPtr is destroyed before it's used to create an
  // AuctionDownloader. `network_events_delegate` is used to call
  // OnNetworkSendRequest only.
  static mojom::InProgressAuctionDownloadPtr StartDownload(
      network::mojom::URLLoaderFactory& url_loader_factory,
      const GURL& source_url,
      MimeType mime_type,
      mojom::AuctionNetworkEventsHandler& network_events_handler,
      std::optional<std::string> post_body = std::nullopt,
      std::optional<std::string> content_type = std::nullopt);

  // Checks if the response is allowed for Protected Audience-related requests,
  // based on the headers. Returns an error string and sets `status_out` on
  // error.
  static std::optional<std::string> CheckResponseAllowed(
      const GURL& url,
      const network::mojom::URLResponseHead& response_head,
      network::URLLoaderCompletionStatus& status_out);

  static std::string_view MimeTypeToStringForTesting(
      AuctionDownloader::MimeType mime_type);

 private:
  // Delegated constructor used by public constructor calls.
  AuctionDownloader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const GURL& source_url,
      DownloadMode download_mode,
      MimeType mime_type,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::optional<std::string> request_id,
      std::optional<std::string> post_body,
      std::optional<std::string> content_type,
      std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
      base::optional_ref<const url::Origin> request_initiator,
      std::optional<network::ResourceRequest::TrustedParams> trusted_params,
      ResponseStartedCallback response_started_callback,
      AuctionDownloaderCallback auction_downloader_callback,
      std::unique_ptr<NetworkEventsDelegate> network_events_delegate);

  void OnHeadersOnlyReceived(scoped_refptr<net::HttpResponseHeaders> headers);

  void OnBodyReceived(std::unique_ptr<std::string> body);

  void OnRedirect(const GURL& url_before_redirect,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);
  void OnResponseStarted(base::Time request_time,
                         const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Notifies tracing, devtools and callback of a failure and cancels any
  // further loading.
  void FailRequest(network::URLLoaderCompletionStatus status,
                   std::string error_string);
  void TraceResult(bool failure,
                   base::TimeTicks completion_time,
                   int64_t encoded_data_length,
                   int64_t decoded_body_length);

  // While revalidating a cached response, keep the SimpleURLLoader
  // alive.
  static void OnRevalidatedBodyReceived(
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      std::unique_ptr<std::string> body) {}

  const raw_ref<network::mojom::URLLoaderFactory> url_loader_factory_;
  const GURL source_url_;
  const MimeType mime_type_;
  const std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1_;
  // A UnguessableToken string to be used in devtools.
  const std::string request_id_;

  // The time the response started, used for UMA.
  std::optional<base::TimeTicks> response_started_time_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  ResponseStartedCallback response_started_callback_;
  AuctionDownloaderCallback auction_downloader_callback_;
  std::unique_ptr<NetworkEventsDelegate> network_events_delegate_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_DOWNLOADER_H_
