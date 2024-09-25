// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {
class InterestGroupManagerImpl;

constexpr char kDefaultBiddingAndAuctionGCPCoordinatorOrigin[] =
    "https://publickeyservice.gcp.privacysandboxservices.com";

struct BiddingAndAuctionServerKey {
  std::string key;  // bytes containing the key.
  uint8_t id;       // key id corresponding to this key.
};

// BiddingAndAuctionServerKeyFetcher manages fetching and caching of the public
// keys for Bidding and Auction Server endpoints from each of the designated
// Coordinators with the provided `loader_factory`. Values are cached both in
// memory and in the database.
class CONTENT_EXPORT BiddingAndAuctionServerKeyFetcher {
 public:
  using BiddingAndAuctionServerKeyFetcherCallback = base::OnceCallback<void(
      base::expected<BiddingAndAuctionServerKey, std::string>)>;

  // `manager` should be the InterestGroupManagerImpl that owns this
  // BiddingAndAuctionServerKeyFetcher.
  BiddingAndAuctionServerKeyFetcher(
      InterestGroupManagerImpl* manager,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~BiddingAndAuctionServerKeyFetcher();
  // no copy
  BiddingAndAuctionServerKeyFetcher(const BiddingAndAuctionServerKeyFetcher&) =
      delete;
  BiddingAndAuctionServerKeyFetcher& operator=(
      const BiddingAndAuctionServerKeyFetcher&) = delete;

  // Fetch keys for all coordinators in kFledgeBiddingAndAuctionKeyConfig if
  // kFledgePrefetchBandAKeys and kFledgeBiddingAndAuctionServer are enabled and
  // if the keys haven't been fetched yet.
  void MaybePrefetchKeys();

  // GetOrFetchKey provides a key in the callback if necessary. If the key is
  // immediately available then the callback may be called synchronously.
  void GetOrFetchKey(const std::optional<url::Origin>& maybe_coordinator,
                     BiddingAndAuctionServerKeyFetcherCallback callback);

 private:
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

    // The time the key fetch starts.
    base::TimeTicks fetch_start;
    // The time the key fetch from the network starts. This time may be after
    // unsuccessfully trying to load the key from the database.
    base::TimeTicks network_fetch_start;

    // loader_ contains the SimpleURLLoader being used to fetch the keys.
    std::unique_ptr<network::SimpleURLLoader> loader;
  };

  // Fetch keys for a particular coordinator, first checking if the key is
  // in the database.
  void FetchKeys(const url::Origin& coordinator,
                 PerCoordinatorFetcherState& state,
                 BiddingAndAuctionServerKeyFetcherCallback callback);

  void OnFetchKeysFromDatabaseComplete(
      const url::Origin& coordinator,
      std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>> keys);

  void FetchKeysFromNetwork(const url::Origin& coordinator);

  // Called when the JSON blob containing the keys have been successfully
  // fetched over the network.
  void OnFetchKeysFromNetworkComplete(url::Origin coordinator,
                                      std::unique_ptr<std::string> response);

  // Called when the JSON blob containing the keys has be parsed into
  // base::Values. Uses the parsed result to add keys to the cache and calls
  // queued callbacks.
  void OnParsedKeys(url::Origin coordinator,
                    data_decoder::DataDecoder::ValueOrError result);

  void CacheKeysAndRunAllCallbacks(
      const url::Origin& coordinator,
      const std::vector<BiddingAndAuctionServerKey>& keys,
      base::Time expiration);

  void FailAllCallbacks(url::Origin coordinator);

  bool did_prefetch_keys_ = false;

  // May be referenced by the fetcher_state_map, so the loader_factory_ should
  // be destructed last.
  const scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  base::flat_map<url::Origin, PerCoordinatorFetcherState> fetcher_state_map_;

  // An unowned pointer to the InterestGroupManagerImpl that owns this
  // BiddingAndAuctionServerKeyFetcher. Used as an intermediary to talk to the
  // database.
  raw_ptr<InterestGroupManagerImpl> manager_;

  const url::Origin default_gcp_coordinator_ =
      url::Origin::Create(GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));

  base::WeakPtrFactory<BiddingAndAuctionServerKeyFetcher> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_
