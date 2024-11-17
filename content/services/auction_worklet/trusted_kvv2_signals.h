// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_KVV2_SIGNALS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_KVV2_SIGNALS_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionDownloader;

// Like the TrustedSignals class, but for version 2 of the Key-Value server API
// for bidding/scoring signals. It fetches and parses the hosted CBOR data
// needed by worklets. There are separate methods for fetching bidding and
// scoring signals. A single `TrustedKVv2Signals` object can only be used to
// fetch either bidding signals or scoring signals, even if a single URL is used
// for both types of signals.
class CONTENT_EXPORT TrustedKVv2Signals {
 public:
  using LoadKVv2SignalsCallback = base::OnceCallback<void(
      std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
          result_map,
      std::optional<std::string> error_msg)>;

  explicit TrustedKVv2Signals(const TrustedKVv2Signals&) = delete;
  TrustedKVv2Signals& operator=(const TrustedKVv2Signals&) = delete;
  ~TrustedKVv2Signals();

  // Construct a TrustedKVv2Signals for fetching bidding signals, and start
  // the fetch. `trusted_bidding_signals_url` must be the base URL (no query
  // params added). Callback will be invoked asynchronously once the data
  // has been fetched or an error has occurred.
  //
  // There are no lifetime constraints of `url_loader_factory`.
  static std::unique_ptr<TrustedKVv2Signals> LoadKVv2BiddingSignals(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      std::set<std::string> interest_group_names,
      std::set<std::string> bidding_signals_keys,
      const GURL& trusted_bidding_signals_url,
      std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
          request_helper_builder,
      scoped_refptr<AuctionV8Helper> v8_helper,
      LoadKVv2SignalsCallback load_kvv2_signals_callback);

  // Just like LoadKVv2BiddingSignals() above, but for fetching seller signals.
  static std::unique_ptr<TrustedKVv2Signals> LoadKVv2ScoringSignals(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      std::set<std::string> render_urls,
      std::set<std::string> ad_component_render_urls,
      const GURL& trusted_scoring_signals_url,
      std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
          request_helper_builder,
      scoped_refptr<AuctionV8Helper> v8_helper,
      LoadKVv2SignalsCallback load_kvv2_signals_callback);

 private:
  // The `context` is generated during the encryption process in the request
  // builder and is saved for decrypting the response body.
  TrustedKVv2Signals(
      std::optional<std::set<std::string>> interest_group_names,
      std::optional<std::set<std::string>> bidding_signals_keys,
      std::optional<std::set<std::string>> render_urls,
      std::optional<std::set<std::string>> ad_component_render_urls,
      const GURL& trusted_signals_url,
      quiche::ObliviousHttpRequest::Context context,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      scoped_refptr<AuctionV8Helper> v8_helper,
      LoadKVv2SignalsCallback load_kvv2_signals_callback);

  // Start downloading `url`, which should be the bidding or scoring signals
  // URL, using the POST method..
  void StartKVv2Download(network::mojom::URLLoaderFactory* url_loader_factory,
                         const GURL& full_signals_url,
                         std::string post_body);

  void OnKVv2DownloadComplete(std::unique_ptr<std::string> body,
                              scoped_refptr<net::HttpResponseHeaders> headers,
                              std::optional<std::string> error_msg);

  // Parse the response body on the V8 thread, and extract values associated
  // with the requested keys.
  static void HandleKVv2DownloadResultOnV8Thread(
      scoped_refptr<AuctionV8Helper> v8_helper,
      const GURL& signals_url,
      std::optional<std::set<std::string>> interest_group_names,
      std::optional<std::set<std::string>> bidding_signals_keys,
      std::optional<std::set<std::string>> render_urls,
      std::optional<std::set<std::string>> ad_component_render_urls,
      std::unique_ptr<std::string> body,
      scoped_refptr<net::HttpResponseHeaders> headers,
      quiche::ObliviousHttpRequest::Context context,
      std::optional<std::string> error_msg,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedKVv2Signals> weak_instance,
      base::TimeDelta download_time);

  // Called from V8 thread.
  static void PostKVv2CallbackToUserThread(
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedKVv2Signals> weak_instance,
      std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
          result_map,
      std::optional<std::string> error_msg);

  // Called on user thread.
  void DeliverKVv2CallbackOnUserThread(
      std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
          result_map,
      std::optional<std::string> error_msg);

  // Keys being fetched. For bidding signals, only `bidding_signals_keys_` and
  // `interest_group_names_` are non-null. For scoring signals, only
  // `render_urls_` and `ad_component_render_urls_` are non-null. These are
  // cleared and ownership is passed to the V8 thread once the download
  // completes, as they're no longer on the main thread after that point.
  std::optional<std::set<std::string>> interest_group_names_;
  std::optional<std::set<std::string>> bidding_signals_keys_;
  std::optional<std::set<std::string>> render_urls_;
  std::optional<std::set<std::string>> ad_component_render_urls_;

  const GURL trusted_signals_url_;
  const scoped_refptr<AuctionV8Helper> v8_helper_;

  LoadKVv2SignalsCallback load_kvv2_signals_callback_;
  std::unique_ptr<AuctionDownloader> auction_downloader_;
  // Used only for metrics; time when download started.
  base::TimeTicks download_start_time_;
  // Save the encryption context for decryption purpose.
  quiche::ObliviousHttpRequest::Context context_;

  mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handler_;

  base::WeakPtrFactory<TrustedKVv2Signals> weak_ptr_factory{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_KVV2_SIGNALS_H_
