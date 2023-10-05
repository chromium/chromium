// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace content {

constexpr char kDefaultBiddingAndAuctionGCPCoordinatorOrigin[] =
    "https://publickeyservice.gcp.privacysandboxservices.com";

struct BiddingAndAuctionServerKey {
  std::string key;  // bytes containing the key.
  uint8_t id;       // key id corresponding to this key.
};

// BiddingAndAuctionServerKeyFetcher Manages fetching and caching of the public
// keys for Bidding and Auction Server endpoints from each of the designated
// Coordinators.
class CONTENT_EXPORT BiddingAndAuctionServerKeyFetcher {
 public:
  using BiddingAndAuctionServerKeyFetcherCallback = base::OnceCallback<void(
      base::expected<BiddingAndAuctionServerKey, std::string>)>;

  BiddingAndAuctionServerKeyFetcher();
  ~BiddingAndAuctionServerKeyFetcher();
  // no copy
  BiddingAndAuctionServerKeyFetcher(const BiddingAndAuctionServerKeyFetcher&) =
      delete;
  BiddingAndAuctionServerKeyFetcher& operator=(
      const BiddingAndAuctionServerKeyFetcher&) = delete;

  // GetOrFetchKey provides a key in the callback, fetching the key over the
  // network with the provided loader_factory if necessary. If the key is
  // immediately available then the callback may be called synchronously.
  void GetOrFetchKey(network::mojom::URLLoaderFactory* loader_factory,
                     absl::optional<url::Origin> maybe_coordinator,
                     BiddingAndAuctionServerKeyFetcherCallback callback);

 private:
  // Called when the JSON blob containing the keys have been successfully
  // fetched over the network.
  void OnFetchKeyComplete(url::Origin coordinator,
                          std::unique_ptr<std::string> response);

  // Called when the JSON blob containing the keys has be parsed into
  // base::Values. Uses the parsed result to add keys to the cache and calls
  // queued callbacks.
  void OnParsedKeys(url::Origin coordinator,
                    data_decoder::DataDecoder::ValueOrError result);
  void FailAllCallbacks(url::Origin coordinator);

  struct PerCoordinatorFetcherState {
    PerCoordinatorFetcherState();
    ~PerCoordinatorFetcherState();

    PerCoordinatorFetcherState(PerCoordinatorFetcherState&& state);
    PerCoordinatorFetcherState& operator=(PerCoordinatorFetcherState&& state);

    GURL key_url;

    // queue_ contains callbacks waiting for a key to be fetched over the
    // network.
    base::circular_deque<BiddingAndAuctionServerKeyFetcherCallback> queue;

    // keys_ contains a list of keys received from the server (if any).
    std::vector<BiddingAndAuctionServerKey> keys;

    // expiration_ contains the expiration time for any keys that are cached by
    // this object.
    base::Time expiration = base::Time::Min();

    // loader_ contains the SimpleURLLoader being used to fetch the keys.
    std::unique_ptr<network::SimpleURLLoader> loader;
  };

  base::flat_map<url::Origin, PerCoordinatorFetcherState> fetcher_state_map_;

  base::WeakPtrFactory<BiddingAndAuctionServerKeyFetcher> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_
