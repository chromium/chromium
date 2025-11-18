// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERVER_KEY_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/interest_group_manager.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {
class InterestGroupManagerImpl;

inline constexpr char kDefaultBiddingAndAuctionGCPCoordinatorOrigin[] =
    "https://publickeyservice.gcp.privacysandboxservices.com";
inline constexpr char kBiddingAndAuctionGCPCoordinatorOrigin[] =
    "https://publickeyservice.pa.gcp.privacysandboxservices.com";
inline constexpr char kBiddingAndAuctionGCPCoordinatorKeyURL[] =
    "https://publickeyservice.pa.gcp.privacysandboxservices.com/.well-known/"
    "protected-auction/v1/public-keys";
inline constexpr char kBiddingAndAuctionAWSCoordinatorOrigin[] =
    "https://publickeyservice.pa.aws.privacysandboxservices.com";
inline constexpr char kBiddingAndAuctionAWSCoordinatorKeyURL[] =
    "https://publickeyservice.pa.aws.privacysandboxservices.com/.well-known/"
    "protected-auction/v1/public-keys";

struct BiddingAndAuctionServerKey {
  std::string key;  // bytes containing the key.
  std::string id;   // key id corresponding to this key.
};

// This class abstracts the set of keys. This makes code accessing the keys more
// generic to ease the transition as we consider alternative key scopes.
// See https://github.com/WICG/turtledove/issues/1334 for details.
class CONTENT_EXPORT BiddingAndAuctionKeySet {
 public:
  explicit BiddingAndAuctionKeySet(
      std::vector<BiddingAndAuctionServerKey> keys);
  explicit BiddingAndAuctionKeySet(
      base::flat_map<url::Origin, std::vector<BiddingAndAuctionServerKey>>
          origin_scoped_keys);
  ~BiddingAndAuctionKeySet();

  BiddingAndAuctionKeySet(BiddingAndAuctionKeySet&& keyset);
  BiddingAndAuctionKeySet& operator=(BiddingAndAuctionKeySet&& keyset);

  // Returns true if we have any keys in this Keyset.
  bool HasKeys() const;
  uint8_t SchemaVersion() const;

  // Returns a random key from the set of keys for this coordinator. If keys are
  // scoped by origin, the provided `scoped_origin` is used to select the the
  // keyset to select from.
  std::optional<BiddingAndAuctionServerKey> GetRandomKey(
      const url::Origin& scoped_origin) const;

  // Convert Keyset to binary Protobuf for storage.
  std::string AsBinaryProto() const;
  // Create a Keyset from binary Protobuf.
  static std::optional<BiddingAndAuctionKeySet> FromBinaryProto(
      std::string key_blob);

 private:
  std::vector<BiddingAndAuctionServerKey> keys_;
  base::flat_map<url::Origin, std::vector<BiddingAndAuctionServerKey>>
      origin_scoped_keys_;
};

// BiddingAndAuctionServerKeyFetcher manages fetching and caching of the public
// keys for Bidding and Auction Server endpoints from each of the designated
// Coordinators with the provided `loader_factory`. Values are cached both in
// memory and in the database.
class CONTENT_EXPORT BiddingAndAuctionServerKeyFetcher {
 public:
  using BiddingAndAuctionServerKeyFetcherCallback = base::OnceCallback<void(
      base::expected<BiddingAndAuctionServerKey, std::string>)>;
  using TrustedServerAPIType = InterestGroupManager::TrustedServerAPIType;

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
  // kFledgeBiddingAndAuctionServer is enabled and if the keys haven't been
  // fetched yet.
  void MaybePrefetchKeys();

  // GetOrFetchKey provides a key in the callback if necessary. If the key is
  // immediately available then the callback may be called synchronously.
  void GetOrFetchKey(TrustedServerAPIType api,
                     const url::Origin& scope_origin,
                     const std::optional<url::Origin>& maybe_coordinator,
                     BiddingAndAuctionServerKeyFetcherCallback callback);

  // Adds a non-database-persistent testing override to key configuration for
  // given `coordinator`. Invokes the callback with an error string if there is
  // a problem, such as if a configuration for a given coordinator already
  // exists. `callback` is called with nullopt on success. Either success or
  // failure may be synchronous or asynchronous.
  using DebugOverrideCallback =
      base::OnceCallback<void(std::optional<std::string>)>;
  void AddKeysDebugOverride(TrustedServerAPIType api,
                            const url::Origin& coordinator,
                            std::string serialized_keys,
                            DebugOverrideCallback callback);

 private:
  struct CallbackQueueItem {
    CallbackQueueItem(BiddingAndAuctionServerKeyFetcherCallback callback,
                      url::Origin scope_origin);
    ~CallbackQueueItem();

    CallbackQueueItem(CallbackQueueItem&& item);
    CallbackQueueItem& operator=(CallbackQueueItem&& item);

    BiddingAndAuctionServerKeyFetcherCallback callback;
    url::Origin scope_origin;
  };

  struct PerCoordinatorFetcherState {
    PerCoordinatorFetcherState();
    ~PerCoordinatorFetcherState();

    PerCoordinatorFetcherState(PerCoordinatorFetcherState&& state);
    PerCoordinatorFetcherState& operator=(PerCoordinatorFetcherState&& state);

    GURL key_url;
    uint8_t version;
    base::EnumSet<TrustedServerAPIType,
                  TrustedServerAPIType::kInvalid,
                  TrustedServerAPIType::kMaxValue>
        apis;

    // If this is set, this is a temporary configuration applied via
    // SetBiddingAndAuctionServerKeysDebugOverride(), and so should not
    // use the DB, and doesn't expire.
    bool debug_override = false;

    // Callback for debug override of config getting parsed. Unlike the
    // normal callbacks in `queue` these are not scoped to origin.
    DebugOverrideCallback debug_override_callback;

    // queue_ contains callbacks waiting for a key to be fetched over the
    // network.
    base::circular_deque<CallbackQueueItem> queue;

    // keys_ contains a list of keys received from the server (if any).
    std::optional<BiddingAndAuctionKeySet> keyset;

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
  void FetchKeys(const url::Origin& scope_origin,
                 const url::Origin& coordinator,
                 PerCoordinatorFetcherState& state,
                 BiddingAndAuctionServerKeyFetcherCallback callback);

  void OnFetchKeysFromDatabaseComplete(const url::Origin& coordinator,
                                       std::pair<base::Time, std::string> keys);

  void FetchKeysFromNetwork(const url::Origin& coordinator);

  // Called when the JSON blob containing the keys have been successfully
  // fetched over the network.
  void OnFetchKeysFromNetworkComplete(url::Origin coordinator,
                                      std::optional<std::string> response);

  // Called when the JSON blob containing the keys has be parsed into
  // base::Values. Uses the parsed result to add keys to the cache and calls
  // queued callbacks.
  void OnParsedKeys(url::Origin coordinator,
                    data_decoder::DataDecoder::ValueOrError result);

  // Called when the JSON blob containing the keys has be parsed into
  // base::Values for v2 keys. Uses the parsed result to add keys to the cache
  // and calls queued callbacks.
  void OnParsedKeysV2(url::Origin coordinator,
                      data_decoder::DataDecoder::ValueOrError result);

  void CacheKeysAndRunAllCallbacks(const url::Origin& coordinator,
                                   BiddingAndAuctionKeySet keyset,
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
