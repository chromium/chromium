// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_cache_impl.h"

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/trusted_signals_fetcher.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// The data stored in each CompressionGroupData.
using CachedResult =
    base::expected<TrustedSignalsFetcher::CompressionGroupResult, std::string>;

// Bind `pending_client` and then send result` to it.
void SendResultToClient(
    mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>
        pending_client,
    const CachedResult& result) {
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCacheClient> client(
      std::move(pending_client));
  if (!client.is_connected()) {
    return;
  }

  if (result.has_value()) {
    client->OnSuccess(result.value().compression_scheme,
                      {result.value().compression_group_data});
  } else {
    client->OnError(result.error());
  }
}

// Sends an error to `pending_client` in the case there's no live cache entry.
// Used both when an unrecognized signals request ID is received, and when the
// last Handle to an entry is destroyed, and there are pending requests to it.
void SendNoLiveEntryErrorToClient(
    mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>
        pending_client) {
  SendResultToClient(std::move(pending_client),
                     base::unexpected("Request cancelled"));
}

}  // namespace

TrustedSignalsCacheImpl::NetworkPartitionNonceKey::NetworkPartitionNonceKey() =
    default;

TrustedSignalsCacheImpl::NetworkPartitionNonceKey::NetworkPartitionNonceKey(
    const url::Origin& script_origin,
    SignalsType signals_type,
    const GURL& trusted_signals_url)
    : script_origin(script_origin),
      signals_type(signals_type),
      trusted_signals_url(trusted_signals_url) {}

TrustedSignalsCacheImpl::NetworkPartitionNonceKey::NetworkPartitionNonceKey(
    const NetworkPartitionNonceKey&) = default;

TrustedSignalsCacheImpl::NetworkPartitionNonceKey::NetworkPartitionNonceKey(
    NetworkPartitionNonceKey&&) = default;

TrustedSignalsCacheImpl::NetworkPartitionNonceKey::~NetworkPartitionNonceKey() =
    default;

TrustedSignalsCacheImpl::NetworkPartitionNonceKey&
TrustedSignalsCacheImpl::NetworkPartitionNonceKey::operator=(
    const NetworkPartitionNonceKey&) = default;

TrustedSignalsCacheImpl::NetworkPartitionNonceKey&
TrustedSignalsCacheImpl::NetworkPartitionNonceKey::operator=(
    NetworkPartitionNonceKey&&) = default;

bool TrustedSignalsCacheImpl::NetworkPartitionNonceKey::operator<(
    const NetworkPartitionNonceKey& other) const {
  return std::tie(script_origin, signals_type, trusted_signals_url) <
         std::tie(other.script_origin, other.signals_type,
                  other.trusted_signals_url);
}

TrustedSignalsCacheImpl::FetchKey::FetchKey() = default;

TrustedSignalsCacheImpl::FetchKey::FetchKey(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    SignalsType signals_type,
    const url::Origin& script_origin,
    const GURL& trusted_signals_url,
    const url::Origin& coordinator)
    : network_partition_nonce_key(script_origin,
                                  signals_type,
                                  trusted_signals_url),
      url_loader_factory(std::move(url_loader_factory)),
      main_frame_origin(main_frame_origin),
      coordinator(coordinator),
      ip_address_space(ip_address_space),
      frame_tree_node_id(frame_tree_node_id) {}

TrustedSignalsCacheImpl::FetchKey::FetchKey(const FetchKey&) = default;
TrustedSignalsCacheImpl::FetchKey::FetchKey(FetchKey&&) = default;

TrustedSignalsCacheImpl::FetchKey& TrustedSignalsCacheImpl::FetchKey::operator=(
    const FetchKey&) = default;
TrustedSignalsCacheImpl::FetchKey& TrustedSignalsCacheImpl::FetchKey::operator=(
    FetchKey&&) = default;

TrustedSignalsCacheImpl::FetchKey::~FetchKey() = default;

bool TrustedSignalsCacheImpl::FetchKey::operator<(const FetchKey& other) const {
  return std::tie(network_partition_nonce_key, url_loader_factory,
                  main_frame_origin, coordinator, ip_address_space,
                  frame_tree_node_id) <
         std::tie(other.network_partition_nonce_key, other.url_loader_factory,
                  other.main_frame_origin, other.coordinator,
                  other.ip_address_space, other.frame_tree_node_id);
}

struct TrustedSignalsCacheImpl::Fetch {
  struct CompressionGroup {
    // The CompressionGroupData corresponding to this fetch. No need to store
    // anything else - the details about the partition can be retrieved when it
    // comes time to make a request from the CacheEntries that
    // `compression_group_data` has iterators for.
    raw_ptr<CompressionGroupData> compression_group_data;

    // Compression group IDs are assigned when a Fetch is started. They are
    // assigned then to more easily handle deletion.
    int compression_group_id = -1;
  };

  // Key used to distinguish compression group. If two *CacheEntries share a
  // FetchKey, whether or not they share a CompressionGroupKey as well
  // determines if they use different compression groups, or use different
  // partitions within a compression group.
  struct CompressionGroupKey {
    // `interest_group_owner_if_scoring_signals` is only needed for scoring
    // signals fetches. For BiddingCacheEntries, it's the same for everything
    // that shares the Fetch, so is not needed.
    CompressionGroupKey(const url::Origin& joining_origin,
                        base::optional_ref<const url::Origin>
                            interest_group_owner_if_scoring_signals)
        : joining_origin(joining_origin),
          interest_group_owner_if_scoring_signals(
              interest_group_owner_if_scoring_signals.CopyAsOptional()) {}

    CompressionGroupKey(CompressionGroupKey&&) = default;

    CompressionGroupKey& operator=(CompressionGroupKey&&) = default;

    bool operator<(const CompressionGroupKey& other) const {
      return std::tie(joining_origin, interest_group_owner_if_scoring_signals) <
             std::tie(other.joining_origin,
                      other.interest_group_owner_if_scoring_signals);
    }

    url::Origin joining_origin;

    std::optional<url::Origin> interest_group_owner_if_scoring_signals;
  };

  using CompressionGroupMap = std::map<CompressionGroupKey, CompressionGroup>;

  explicit Fetch(TrustedSignalsCacheImpl* trusted_signals_cache)
      : weak_ptr_factory(trusted_signals_cache) {}

  // Adds devtools auction ID to the Fetch, if the Fetch hasn't yet started.
  // Note that devtools IDs cannot currently be removed from fetches, to make
  // bookkeeping simpler, and are only logged on fetch start.
  void AddDevtoolsAuctionId(const std::string& devtools_auction_id) {
    // If fetch has started, do nothing.
    if (fetcher) {
      return;
    }

    devtools_auction_ids.insert(devtools_auction_id);
  }

  CompressionGroupMap compression_groups;

  std::unique_ptr<TrustedSignalsFetcher> fetcher;

  // Before a request can be started, `can_start` must be true, and it must have
  // a `coordinator_key`. `can_start` can be set by the caller, or is
  // automatically set on a delay for sellers, and `coordinator_key` is
  // retrieved by the GetCoordinatorKeyCallback.
  bool can_start = false;
  std::optional<BiddingAndAuctionServerKey> coordinator_key;

  // Devtools IDs of all associated auctions. They're all logged on fetch start,
  // and the set is permanently cleared. Use `flat_set` because expected use
  // case is a few Fetches shared by a lot of IGs in a small number of auctions,
  // so most insertion attempts should not modify the set, and the better lookup
  // performance seems more likely to matter.
  base::flat_set<std::string> devtools_auction_ids;

  // Weak reference to the TrustedSignalsCacheImpl. Used for calls to
  // GetCoordinatorKeyCallback, and delayed calls to set `can_start` to true, so
  // that destroying the fetch aborts the callback.
  base::WeakPtrFactory<TrustedSignalsCacheImpl> weak_ptr_factory;
};

TrustedSignalsCacheImpl::BiddingCacheKey::BiddingCacheKey() = default;

TrustedSignalsCacheImpl::BiddingCacheKey::BiddingCacheKey(
    const url::Origin& interest_group_owner,
    std::optional<std::string> interest_group_name,
    const GURL& trusted_signals_url,
    const url::Origin& coordinator,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    const url::Origin& joining_origin,
    base::Value::Dict additional_params,
    base::optional_ref<const std::string> buyer_tkv_signals)
    : interest_group_name(std::move(interest_group_name)),
      fetch_key(std::move(url_loader_factory),
                frame_tree_node_id,
                main_frame_origin,
                ip_address_space,
                SignalsType::kBidding,
                interest_group_owner,
                trusted_signals_url,
                coordinator),
      joining_origin(joining_origin),
      additional_params(std::move(additional_params)),
      buyer_tkv_signals(buyer_tkv_signals.CopyAsOptional()) {}

TrustedSignalsCacheImpl::BiddingCacheKey::BiddingCacheKey(BiddingCacheKey&&) =
    default;

TrustedSignalsCacheImpl::BiddingCacheKey::~BiddingCacheKey() = default;

TrustedSignalsCacheImpl::BiddingCacheKey&
TrustedSignalsCacheImpl::BiddingCacheKey::operator=(BiddingCacheKey&&) =
    default;

bool TrustedSignalsCacheImpl::BiddingCacheKey::operator<(
    const BiddingCacheKey& other) const {
  return std::tie(interest_group_name, fetch_key, joining_origin,
                  additional_params, buyer_tkv_signals) <
         std::tie(other.interest_group_name, other.fetch_key,
                  other.joining_origin, other.additional_params,
                  other.buyer_tkv_signals);
}

struct TrustedSignalsCacheImpl::BiddingCacheEntry {
  BiddingCacheEntry(const std::string& interest_group_name,
                    bool is_group_by_origin,
                    base::optional_ref<const std::vector<std::string>>
                        trusted_bidding_signals_keys)
      : interest_group_names{interest_group_name},
        is_group_by_origin(is_group_by_origin) {
    if (trusted_bidding_signals_keys.has_value()) {
      keys.insert(trusted_bidding_signals_keys->begin(),
                  trusted_bidding_signals_keys->end());
    }
  }

  // Returns `true` if `interest_group_names` contains `interest_group_name` and
  // `keys` contains all elements of `trusted_bidding_signals_keys`. The latter
  // is considered true if `trusted_bidding_signals_keys` is nullopt or empty.
  // Expects the BiddingCacheKey to already have been checked, so ignore
  // `interest_group_name` if `is_group_by_origin` is true, though does DCHECK
  // if `is_group_by_origin` is true but `interest_group_names` does not contain
  // `interest_group_name`.
  bool ContainsInterestGroup(const std::string& interest_group_name,
                             base::optional_ref<const std::vector<std::string>>
                                 trusted_bidding_signals_keys) const {
    if (is_group_by_origin) {
      if (!interest_group_names.contains(interest_group_name)) {
        return false;
      }
    } else {
      DCHECK_EQ(1u, interest_group_names.size());
      DCHECK(interest_group_names.contains(interest_group_name));
    }

    if (trusted_bidding_signals_keys.has_value()) {
      for (const auto& key : *trusted_bidding_signals_keys) {
        if (!keys.contains(key)) {
          return false;
        }
      }
    }
    return true;
  }

  // Adds `interest_group_name` into `interest_group_names`, if
  // `is_group_by_origin` is false, otherwise DCHECKs if it's not already the
  // only entry in `interest_group_names`. Also, if
  // `trusted_bidding_signals_keys` is non-null, merges it into `keys`.
  void AddInterestGroup(const std::string& interest_group_name,
                        base::optional_ref<const std::vector<std::string>>
                            trusted_bidding_signals_keys) {
    if (is_group_by_origin) {
      interest_group_names.emplace(interest_group_name);
    } else {
      DCHECK_EQ(1u, interest_group_names.size());
      DCHECK(interest_group_names.contains(interest_group_name));
    }

    if (trusted_bidding_signals_keys.has_value()) {
      keys.insert(trusted_bidding_signals_keys->begin(),
                  trusted_bidding_signals_keys->end());
    }
  }

  // Names of all interest groups in this CacheEntry. If this entry is
  // a group-by-origin cluster of interest groups, with a nullopt
  // `interest_group_name` key, this may contain multiple interest group names.
  // Otherwise, contains the same name as BiddingCacheKey::interest_group_name
  // and no others.
  std::set<std::string> interest_group_names;

  std::set<std::string> keys;

  // A pointer to the associated CompressionGroupData. When the
  // CompressionGroupData is destroyed, `this` will be as well.
  raw_ptr<CompressionGroupData> compression_group_data;

  // Partition within the CompressionGroupData corresponding to this CacheEntry.
  // All CacheEntries with the same CompressionGroupData have unique
  // `partition_ids`.  Default value should never be used.
  int partition_id = 0;

  // Whether this entry is a group-by-origin entry or not. Group-by-origin
  // entries may contain multiple interest groups with group-by-origin mode
  // enabled, all joined by the same origin, while non-group-by-origin entries
  // may only contain a single interest group (though if re-joined from the same
  // origin, they can theoretically contain merged different versions of the
  // same interest group).
  bool is_group_by_origin = false;
};

TrustedSignalsCacheImpl::ScoringCacheKey::ScoringCacheKey() = default;

TrustedSignalsCacheImpl::ScoringCacheKey::ScoringCacheKey(
    const url::Origin& seller,
    const GURL& trusted_signals_url,
    const url::Origin& coordinator,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    const url::Origin& interest_group_owner,
    const url::Origin& joining_origin,
    const GURL& render_url,
    const std::vector<GURL>& component_render_urls,
    base::Value::Dict additional_params,
    base::optional_ref<const std::string> seller_tkv_signals)
    : render_url(render_url),
      component_render_urls(component_render_urls.begin(),
                            component_render_urls.end()),
      fetch_key(std::move(url_loader_factory),
                frame_tree_node_id,
                main_frame_origin,
                ip_address_space,
                SignalsType::kScoring,
                seller,
                trusted_signals_url,
                coordinator),
      joining_origin(joining_origin),
      interest_group_owner(interest_group_owner),
      additional_params(std::move(additional_params)),
      seller_tkv_signals(seller_tkv_signals.CopyAsOptional()) {}

TrustedSignalsCacheImpl::ScoringCacheKey::ScoringCacheKey(ScoringCacheKey&&) =
    default;

TrustedSignalsCacheImpl::ScoringCacheKey::~ScoringCacheKey() = default;

TrustedSignalsCacheImpl::ScoringCacheKey&
TrustedSignalsCacheImpl::ScoringCacheKey::operator=(ScoringCacheKey&&) =
    default;

bool TrustedSignalsCacheImpl::ScoringCacheKey::operator<(
    const ScoringCacheKey& other) const {
  return std::tie(render_url, component_render_urls, fetch_key, joining_origin,
                  interest_group_owner, additional_params, seller_tkv_signals) <
         std::tie(other.render_url, other.component_render_urls,
                  other.fetch_key, other.joining_origin,
                  other.interest_group_owner, other.additional_params,
                  other.seller_tkv_signals);
}

struct TrustedSignalsCacheImpl::ScoringCacheEntry {
  // Unlike BiddingCacheEntries, ScoringCacheEntries are currently indexed by
  // all their request parameters, so the constructor doesn't need any
  // arguments.
  ScoringCacheEntry() = default;

  // A pointer to the associated CompressionGroupData. When the
  // CompressionGroupData is destroyed, `this` will be as well.
  raw_ptr<CompressionGroupData> compression_group_data;

  // Partition within the CompressionGroupData corresponding to this CacheEntry.
  // All CacheEntries with the same CompressionGroupData have unique
  // `partition_ids`.  Default value should never be used.
  int partition_id = 0;
};

class TrustedSignalsCacheImpl::CompressionGroupData
    : public base::RefCounted<CompressionGroupData> {
 public:
  // Creates a CompressionGroupData.
  //
  // In addition to owning the Fetch (possibly jointly with other
  // CompressionGroupData objects) and the CachedResult once the fetch
  // completes, CompressionGroupData tracks and implicitly owns the CacheEntries
  // associated with the data..
  //
  // `cache` must outlive the created object,
  // and `fetch` must remain valid until the CompressionGroupData is destroyed
  // or SetData() is invoked.
  //
  // `receiver_restrictions` restrict which pipes may request data from the
  // CompressionGroup.
  //
  // `fetch` and `fetch_compression_group` are iterators to the pending fetch
  // that will populate the CompressionGroupData, and the compression group
  // within that fetch that corresponds to the created CompressionGroupData.
  //
  // Informs `cache` when it's destroyed, so all references must be released
  // before the TrustedSignalsCacheImpl is destroyed.
  CompressionGroupData(
      TrustedSignalsCacheImpl* cache,
      ReceiverRestrictions receiver_restrictions,
      FetchMap::iterator fetch,
      Fetch::CompressionGroupMap::iterator fetch_compression_group)
      : cache_(cache),
        receiver_restrictions_(std::move(receiver_restrictions)),
        fetch_(fetch),
        fetch_compression_group_(fetch_compression_group) {}

  // Sets the received data and sets `size_` accordingly. May only be called
  // once. Clears information about the Fetch, since it's now completed.
  //
  // Also sends `data` to all pending clients waiting on it, if there are any,
  // and clears them all.
  void SetData(CachedResult data) {
    DCHECK(!data_);
    DCHECK_EQ(size_, 0u);
    data_ = std::make_unique<CachedResult>(std::move(data));

    // Errors are given TTLs of 0.
    if (!data_->has_value()) {
      expiry_ = base::TimeTicks::Now();
    } else {
      expiry_ = base::TimeTicks::Now() + data_->value().ttl;

      // Calculate size. Leave it as zero on errors, since errors are instantly
      // expired and won't be added to the LruList, anyways.
      //
      // TODO(https://crbug.com/333445540): Consider including more in this
      // measurement, and including the size of everything other than
      // `compression_group_data` in its value on construction.
      size_ = sizeof(CompressionGroupData) +
              (*data_)->compression_group_data.size();
    }

    // The fetch has now completed and the caller will delete it once it's done
    // sending the data to any consumers.
    fetch_ = std::nullopt;
    fetch_compression_group_ = std::nullopt;

    // Send data to pending clients.
    for (auto& pending_client : pending_clients_) {
      SendResultToClient(std::move(pending_client), *data_);
    }
    pending_clients_.clear();
  }

  // True if SetData() has been invoked.
  bool has_data() const { return !!data_; }

  // May only be called if has_data() returns true.
  const CachedResult& data() const { return *data_; }

  const ReceiverRestrictions& receiver_restrictions() const {
    return receiver_restrictions_;
  }

  // Returns true if the data has expired. If there's still a pending fetch,
  // `expiry_` won't have been set yet, but the data is considered not to be
  // expired.
  bool IsExpired() const {
    if (fetch_) {
      return false;
    }
    return *expiry_ <= base::TimeTicks::Now();
  }

  // Associates a BiddingCacheEntry with the CompressionGroupData. When the
  // CompressionGroupData is destroyed, this is used by the cache to destroy all
  // associated CacheEntries.
  void AddBiddingEntry(BiddingCacheEntryMap::iterator bidding_cache_entry) {
    // `this` may only have bidding or scoring signals, not both.
    DCHECK(scoring_cache_entries_.empty());
    DCHECK_EQ(receiver_restrictions_.signals_type, SignalsType::kBidding);

    bidding_cache_entries_.emplace(bidding_cache_entry->second.partition_id,
                                   bidding_cache_entry);
  }

  // Associates a ScoringCacheEntry with the CompressionGroupData. When the
  // CompressionGroupData is destroyed, this is used by the cache to destroy all
  // associated CacheEntries.
  void AddScoringEntry(ScoringCacheEntryMap::iterator scoring_cache_entry) {
    // `this` may only have bidding or scoring signals, not both.
    DCHECK(bidding_cache_entries_.empty());
    DCHECK_EQ(receiver_restrictions_.signals_type, SignalsType::kScoring);

    scoring_cache_entries_.emplace(scoring_cache_entry->second.partition_id,
                                   scoring_cache_entry);
  }

  // Removes `bidding_cache_entry` from `bidding_cache_entries_`.
  // `bidding_cache_entry` must be present in `bidding_cache_entries_`.
  void RemoveBiddingCacheEntry(BiddingCacheEntry* bidding_cache_entry) {
    CHECK_EQ(1u,
             bidding_cache_entries_.erase(bidding_cache_entry->partition_id));
  }

  // Removes `scoring_cache_entry` from `scoring_cache_entries_`.
  // `scoring_cache_entry` must be present in `scoring_cache_entries_`.
  void RemoveScoringCacheEntry(ScoringCacheEntry* scoring_cache_entry) {
    CHECK_EQ(1u,
             scoring_cache_entries_.erase(scoring_cache_entry->partition_id));
  }

  // Contains iterators to associated BiddingCacheEntries, indexed by partition
  // ID.
  const std::map<int, BiddingCacheEntryMap::iterator>& bidding_cache_entries()
      const {
    return bidding_cache_entries_;
  }

  // Contains iterators to associated ScoringCacheEntries, indexed by partition
  // ID.
  const std::map<int, ScoringCacheEntryMap::iterator>& scoring_cache_entries()
      const {
    return scoring_cache_entries_;
  }

  // The Fetch associated with the CompressionGroup, if the Fetch has not yet
  // completed. It may or may not be started. May only be called before the
  // Fetch completes.
  FetchMap::iterator fetch() const {
    DCHECK(fetch_);
    return *fetch_;
  }

  // The CompressionGroup of the Fetch associated with `this`. May only be
  // called before the Fetch completes.
  Fetch::CompressionGroupMap::iterator fetch_compression_group() const {
    DCHECK(fetch_compression_group_);
    return *fetch_compression_group_;
  }

  void AddPendingClient(
      mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>
          pending_client) {
    pending_clients_.emplace_back(std::move(pending_client));
  }

  std::vector<
      mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>>
  TakePendingClients() {
    return std::move(pending_clients_);
  }

  // Returns the ID for the next partition. Technically could use
  // `bidding_cache_entries_.size()`, since BiddingCacheEntries can can only be
  // added to the compression group before its fetch starts, and can only be
  // removed from a compression group (thus reducing size()) after the group's
  // Fetch starts, but safest to track this separately.
  int GetNextPartitionId() { return next_partition_id_++; }

  void StartFetch() {
    // If there's no fetch,  the fetch has already completed (or failed), so
    // there's nothing to do.
    if (!fetch_) {
      return;
    }

    cache_->SetFetchCanStart(*fetch_);
  }

  base::UnguessableToken compression_group_token() const {
    return compression_group_token_;
  }

  // Used, along with total size of all compression groups, to determine if an
  // entry should be added to the LruList or discarded.
  bool IsLoadedAndCanBeUsedByNewRequests() const {
    // If there's a Fetch, then the data hasn't finished loading yet.
    if (fetch_) {
      return false;
    }

    // If expired, can't be used by new requests. Note that failed fetches are
    // considered instantly expired, so will always fail this check.
    if (IsExpired()) {
      return false;
    }

    // Return whether there are any entries in the index that point at `this`.
    return !bidding_cache_entries_.empty() || !scoring_cache_entries_.empty();
  }

  // The approximate size of the entry. Starts at 0, and only set to anything
  // else in SetData().
  size_t size() const { return size_; }

  // Called by Handle on construction. The Handle must be holding onto a
  // reference to the CompressionGroupData, as this will remove it from the
  // LruList, if it's in the list, so would otherwise result in destruction of
  // `this`.
  void OnHandleCreated() {
    if (lru_list_it_) {
      cache_->RemoveFromLruList(*lru_list_it_);
    }
    ++num_handles_;
  }

  // Called by Handle's destructor. Returns number of outstanding Handles. If
  // it's 0, the Handle should inform the TrustedSignalsCacheImpl.
  uint32_t OnHandleDestroyed() {
    // All Handles should be destroyed before this is added to the LruList.
    DCHECK(!lru_list_it_);
    return --num_handles_;
  }

  // Sets `lru_list_it_` and calculates `scheduled_cleanup_time_`, though the
  // caller is responsible for destroying `this` once the cleanup time is
  // reached.
  void SetLruListIt(LruList::iterator lru_list_it) {
    // There should be no Handles when calling this method - it should be
    // removed from the LruList before any handle is created.
    DCHECK_EQ(num_handles_, 0u);
    DCHECK(!lru_list_it_);
    DCHECK(!scheduled_cleanup_time_);

    lru_list_it_ = std::move(lru_list_it);
    scheduled_cleanup_time_ = base::TimeTicks::Now() + kMinUnusedCleanupTime;
  }

  // Clears `lru_list_it_` and `scheduled_cleanup_time_`. Should be called when
  // removing `this` from the LruList, either for use by a Handle or in order to
  // destroy it.
  void ClearLruListIt() {
    // This should be called before adding any handles.
    DCHECK_EQ(num_handles_, 0u);

    lru_list_it_ = std::nullopt;
    scheduled_cleanup_time_ = std::nullopt;
  }

  // Returns `scheduled_cleanup_time_`. To call this method, `this` must be in
  // the LruList, and thus have a scheduled removal time.
  base::TimeTicks scheduled_cleanup_time() const {
    return *scheduled_cleanup_time_;
  }

 private:
  friend class base::RefCounted<CompressionGroupData>;

  virtual ~CompressionGroupData() {
    // The CompressionGroupData should be have no Handles and not be in the LRU
    // list when destroyed.
    DCHECK_EQ(num_handles_, 0u);
    DCHECK(lru_list_it_ == std::nullopt);

    cache_->OnCompressionGroupDataDestroyed(*this);
  }

  const raw_ptr<TrustedSignalsCacheImpl> cache_;

  // Restrictions on what receivers can use this cache entry.
  const ReceiverRestrictions receiver_restrictions_;

  // Information about a pending or live Fetch. Iterators make it convenient for
  // TrustedSignalsCacheImpl::OnCompressionGroupDataDestroyed() to remove the
  // corresponding objects on cancellation, if needed, both in terms of
  // performance and in terms of not having to worry about the keys for the
  // corresponding maps in this class.
  //
  // Cleared when TrustedSignalsCacheImpl::OnFetchComplete() calls SetData().
  // OnFetchComplete() will also delete the underlying Fetch.
  std::optional<FetchMap::iterator> fetch_;
  std::optional<Fetch::CompressionGroupMap::iterator> fetch_compression_group_;

  std::unique_ptr<CachedResult> data_;

  // Expiration time. Populated when `data_` is set.
  std::optional<base::TimeTicks> expiry_;

  // When `this` should be removed from the LruList to reduce memory usage. Only
  // set when added to the LRUCache.
  std::optional<base::TimeTicks> scheduled_cleanup_time_;

  // All *CacheEntries associated with this CompressionGroupData. The maps are
  // indexed by partition ID. Each CompressionGroupData may only have bidding or
  // scoring cache entries, as bidding and scoring fetches are never combined.
  //
  //
  // Using a map allows for log(n) removal from this map when a *CacheEntry is
  // individually destroyed, tracking iterators allows for O(1) removal from the
  // TrustedSignalsCacheImpl's maps of all *CacheEntries when the
  // CompressionGroupData is destroyed.
  //
  // Iterators are also needed because the Fetch needs access to the *CacheKeys.
  std::map<int, BiddingCacheEntryMap::iterator> bidding_cache_entries_;
  std::map<int, ScoringCacheEntryMap::iterator> scoring_cache_entries_;

  // Requests for this cache entry. Probably not worth binding them to watch for
  // cancellation, since can't cancel unless there's no handle, at which point,
  // pending requests can all be ignored, anyways.
  std::vector<
      mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>>
      pending_clients_;

  int next_partition_id_ = 0;

  // The number of Handles. Maintained by calls made from Handle's
  // constructor/destructor. When non-zero, `lru_list_it_` should be nullopt.
  // This is used to determine when an entry should be added to the LruList.
  // While CompressionGroupData is refcounted, and the reference count could
  // theoretically be used for this purpose, references may be owned by things
  // other than Handles as well, so it's safest to maintain this count
  // separately.
  uint32_t num_handles_ = 0;

  // Corresponding entry in `cache_->lru_list_`, if there currently is one.
  std::optional<LruList::iterator> lru_list_it_;

  // Approximate size of the entry. Calculated when the fetch completes.
  size_t size_ = 0;

  // The token that needs to be passed to GetTrustedSignals() to retrieve the
  // response.
  const base::UnguessableToken compression_group_token_{
      base::UnguessableToken::Create()};
};

TrustedSignalsCacheImpl::Handle::Handle(
    TrustedSignalsCacheImpl* trusted_signals_cache,
    scoped_refptr<CompressionGroupData> compression_group_data)
    : trusted_signals_cache_(trusted_signals_cache),
      compression_group_data_(std::move(compression_group_data)) {
  compression_group_data_->OnHandleCreated();
}

TrustedSignalsCacheImpl::Handle::~Handle() {
  if (compression_group_data_->OnHandleDestroyed() == 0u) {
    trusted_signals_cache_->OnLastHandleDestroyed(
        std::move(compression_group_data_));
  }
}

base::UnguessableToken
TrustedSignalsCacheImpl::Handle::compression_group_token() const {
  return compression_group_data_->compression_group_token();
}

void TrustedSignalsCacheImpl::Handle::StartFetch() {
  compression_group_data_->StartFetch();
}

bool TrustedSignalsCacheImpl::ReceiverRestrictions::operator==(
    const ReceiverRestrictions& other) const = default;

TrustedSignalsCacheImpl::TrustedSignalsCacheImpl(
    DataDecoderManager* data_decoder_manager,
    GetCoordinatorKeyCallback get_coordinator_key_callback)
    : data_decoder_manager_(data_decoder_manager),
      get_coordinator_key_callback_(std::move(get_coordinator_key_callback)),
      network_partition_nonce_cache_(kNonceCacheSize) {
  DCHECK(data_decoder_manager_);
}

TrustedSignalsCacheImpl::~TrustedSignalsCacheImpl() {
  // Clearing the LruList should delete all remaining compression group entries.
  // Need to call RemoveFromLruList() for each entry to avoid DCHECKs in
  // CompressionGroupData's destructor, due to `lru_list_it_` not being cleared
  // before destruction.
  while (!lru_list_.empty()) {
    RemoveFromLruList(lru_list_.begin());
  }
  lru_list_.clear();
  DCHECK(compression_group_data_map_.empty());
  DCHECK_EQ(size_, 0u);
}

mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCache>
TrustedSignalsCacheImpl::CreateRemote(SignalsType signals_type,
                                      const url::Origin& script_origin) {
  mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCache> out;
  receiver_set_.Add(this, out.InitWithNewPipeAndPassReceiver(),
                    ReceiverRestrictions{signals_type, script_origin});
  return out;
}

std::unique_ptr<TrustedSignalsCacheImpl::Handle>
TrustedSignalsCacheImpl::RequestTrustedBiddingSignals(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    const std::string& devtools_auction_id,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    const url::Origin& interest_group_owner,
    const std::string& interest_group_name,
    blink::mojom::InterestGroup_ExecutionMode execution_mode,
    const url::Origin& joining_origin,
    const GURL& trusted_signals_url,
    const url::Origin& coordinator,
    base::optional_ref<const std::vector<std::string>>
        trusted_bidding_signals_keys,
    base::Value::Dict additional_params,
    base::optional_ref<const std::string> buyer_tkv_signals,
    int& partition_id) {
  bool is_group_by_origin =
      execution_mode ==
      blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
  BiddingCacheKey cache_key(
      interest_group_owner,
      is_group_by_origin ? std::nullopt
                         : std::make_optional(interest_group_name),
      trusted_signals_url, coordinator, std::move(url_loader_factory),
      frame_tree_node_id, main_frame_origin, ip_address_space, joining_origin,
      std::move(additional_params), buyer_tkv_signals);

  BiddingCacheEntryMap::iterator cache_entry_it =
      bidding_cache_entries_.find(cache_key);
  if (cache_entry_it != bidding_cache_entries_.end()) {
    BiddingCacheEntry* cache_entry = &cache_entry_it->second;
    CompressionGroupData* compression_group_data =
        cache_entry->compression_group_data;

    // If `cache_entry`'s Fetch hasn't yet started, update the BiddingCacheEntry
    // to include any new keys, and return the entry's CompressionGroupData. The
    // Fetch will get the updated keys when it's started, so it does not need to
    // be modified, other than adding `devtools_auction_id`.
    if (!compression_group_data->has_data() &&
        !compression_group_data->fetch()->second.fetcher) {
      cache_entry->AddInterestGroup(interest_group_name,
                                    trusted_bidding_signals_keys);
      compression_group_data->fetch()->second.AddDevtoolsAuctionId(
          devtools_auction_id);
      partition_id = cache_entry->partition_id;
      return std::make_unique<Handle>(this,
                                      scoped_refptr(compression_group_data));
    }

    // Otherwise, check if the entry is not expired and all necessary values
    // that aren't part of the BiddingCacheKey appear in the entry. If both are
    // the case, reuse the cache entry without doing any more work.
    if (!compression_group_data->IsExpired() &&
        cache_entry->ContainsInterestGroup(interest_group_name,
                                           trusted_bidding_signals_keys)) {
      partition_id = cache_entry->partition_id;
      return std::make_unique<Handle>(this, compression_group_data);
    }

    // Otherwise, delete the cache entry. Even if its `compression_group_data`
    // is still in use, this is fine, as the CacheEntry only serves two
    // purposes: 1) It allows new requests to find the entry. 2) It's used to
    // populate fields for the Fetch.
    //
    // 1) doesn't create any issues - the new entry will be returned instead, if
    // it's usable. 2) is also not a problem, since we checked just above if
    // there was a Fetch that hadn't started yet, and if so, reused the entry.
    //
    // This behavior allows `bidding_cache_entries_` to be a map instead of a
    // multimap, to avoid having to worry about multiple live fetches. This path
    // should be uncommon - it's only hit when an interest group is modified, or
    // a group-by-origin IG is joined between auctions.
    DestroyBiddingCacheEntry(cache_entry_it);
  }

  // If there was no matching cache entry, create a new one, and set up the
  // Fetch.

  // Create a new cache entry, moving `cache_key` and creating a CacheEntry
  // in-place.
  cache_entry_it = bidding_cache_entries_
                       .emplace(std::piecewise_construct,
                                std::forward_as_tuple(std::move(cache_key)),
                                std::forward_as_tuple(
                                    interest_group_name, is_group_by_origin,
                                    trusted_bidding_signals_keys))
                       .first;

  scoped_refptr<CompressionGroupData> compression_group_data =
      FindOrCreateCompressionGroupDataAndQueueFetch(
          cache_entry_it->first.fetch_key, cache_entry_it->first.joining_origin,
          devtools_auction_id,
          /*interest_group_owner_if_scoring_signals=*/std::nullopt);

  // The only thing left to do is set up pointers so objects can look up each
  // other and return the result. When it's time to send a request, the Fetch
  // can look up the associated CacheEntries for each compression group to get
  // the data it needs to pass on.

  cache_entry_it->second.compression_group_data = compression_group_data.get();

  // Note that partition ID must be assigned before adding the entry to the
  // CompressionGroupData, since CompressionGroupData uses the partition ID as
  // the index.
  cache_entry_it->second.partition_id =
      compression_group_data->GetNextPartitionId();
  compression_group_data->AddBiddingEntry(cache_entry_it);

  partition_id = cache_entry_it->second.partition_id;
  return std::make_unique<Handle>(this, std::move(compression_group_data));
}

std::unique_ptr<TrustedSignalsCacheImpl::Handle>
TrustedSignalsCacheImpl::RequestTrustedScoringSignals(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    const std::string& devtools_auction_id,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    const url::Origin& seller,
    const GURL& trusted_signals_url,
    const url::Origin& coordinator,
    const url::Origin& interest_group_owner,
    const url::Origin& joining_origin,
    const GURL& render_url,
    const std::vector<GURL>& component_render_urls,
    base::Value::Dict additional_params,
    base::optional_ref<const std::string> seller_tkv_signals,
    int& partition_id) {
  ScoringCacheKey cache_key(
      seller, trusted_signals_url, coordinator, std::move(url_loader_factory),
      frame_tree_node_id, main_frame_origin, ip_address_space,
      interest_group_owner, joining_origin, render_url, component_render_urls,
      std::move(additional_params), seller_tkv_signals);

  ScoringCacheEntryMap::iterator cache_entry_it =
      scoring_cache_entries_.find(cache_key);
  if (cache_entry_it != scoring_cache_entries_.end()) {
    ScoringCacheEntry* cache_entry = &cache_entry_it->second;
    CompressionGroupData* compression_group_data =
        cache_entry->compression_group_data;

    // As long as the data hasn't expired (including the case it hasn't been
    // fetched yet), can reuse the matching ScoringCacheEntry. Unlike with
    // BiddingCacheEntries, there's never a need to modify the CacheEntry, since
    // all parameters are in the key, which must match exactly.
    if (!compression_group_data->has_data() ||
        !compression_group_data->IsExpired()) {
      // If there's a pending fetch, need to call AddDevtoolsAuctionId().
      if (!compression_group_data->has_data()) {
        compression_group_data->fetch()->second.AddDevtoolsAuctionId(
            devtools_auction_id);
      }
      partition_id = cache_entry->partition_id;
      return std::make_unique<Handle>(this,
                                      scoped_refptr(compression_group_data));
    }

    // Otherwise, delete the cache entry. Even if its `compression_group_data`
    // is still in use, this is fine, as the CacheEntry only serves two
    // purposes: 1) It allows new requests to find the entry. 2) It's used to
    // populate fields for the Fetch.
    //
    // 1) doesn't create any issues - the new entry will be returned instead, if
    // it's usable. 2) is also not a problem, since we checked just above if
    // there was a Fetch that hadn't started yet, and if so, reused the entry.
    //
    // This behavior allows `scoring_cache_entries_` to be a map instead of a
    // multimap.
    DestroyScoringCacheEntry(cache_entry_it);
  }

  // If there was no matching cache entry, create a new one, and set up the
  // Fetch.

  // Create a new cache entry, moving `cache_key` and creating a CacheEntry
  // in-place.
  cache_entry_it =
      scoring_cache_entries_.try_emplace(std::move(cache_key)).first;

  scoped_refptr<CompressionGroupData> compression_group_data =
      FindOrCreateCompressionGroupDataAndQueueFetch(
          cache_entry_it->first.fetch_key, cache_entry_it->first.joining_origin,
          devtools_auction_id, interest_group_owner);

  // The only thing left to do is set up pointers so objects can look up each
  // other and return the result. When it's time to send a request, the Fetch
  // can look up the associated CacheEntries for each compression group to get
  // the data it needs to pass on.

  cache_entry_it->second.compression_group_data = compression_group_data.get();

  // Note that partition ID must be assigned before adding the entry to the
  // CompressionGroupData, since CompressionGroupData uses the partition ID as
  // the index.
  cache_entry_it->second.partition_id =
      compression_group_data->GetNextPartitionId();
  compression_group_data->AddScoringEntry(cache_entry_it);

  partition_id = cache_entry_it->second.partition_id;
  return std::make_unique<Handle>(this, scoped_refptr(compression_group_data));
}

scoped_refptr<TrustedSignalsCacheImpl::CompressionGroupData>
TrustedSignalsCacheImpl::FindOrCreateCompressionGroupDataAndQueueFetch(
    const FetchKey& fetch_key,
    const url::Origin& joining_origin,
    const std::string& devtools_auction_id,
    base::optional_ref<const url::Origin>
        interest_group_owner_if_scoring_signals) {
  // If there are any Fetches with the correct FetchKey, check if the last one
  // is still pending. If so, reuse it. Otherwise, will need to create a new
  // Fetch. Don't need to check the others because multimaps insert in FIFO
  // order, and so this logic ensures that only the most recent fetch may not
  // have been started yet.
  auto [first, end] = fetches_.equal_range(fetch_key);
  FetchMap::iterator fetch_it = fetches_.end();
  if (first != end) {
    auto last = std::prev(end, 1);
    if (!last->second.fetcher) {
      fetch_it = last;
    }
  }

  if (fetch_it == fetches_.end()) {
    fetch_it = fetches_.emplace(std::piecewise_construct,
                                std::forward_as_tuple(fetch_key),
                                std::forward_as_tuple(this));

    // If the fetch is new, post a task to get the coordinator key. Since
    // GetCoordinatorKey can complete synchronously with an error, which results
    // in resolving the fetch, it's not safe to call it immediately.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TrustedSignalsCacheImpl::GetCoordinatorKey,
                       fetch_it->second.weak_ptr_factory.GetWeakPtr(),
                       fetch_it));

    // Automatically start fetch if no consumer starts it soon enough.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TrustedSignalsCacheImpl::SetFetchCanStart,
                       fetch_it->second.weak_ptr_factory.GetWeakPtr(),
                       fetch_it),
        kAutoStartDelay);
  }

  Fetch* fetch = &fetch_it->second;
  fetch->AddDevtoolsAuctionId(devtools_auction_id);

  // Now that we have a matching Fetch, check if there's an existing compression
  // group that can be reused.
  auto [compression_group_it, new_element_created] =
      fetch->compression_groups.try_emplace(
          {joining_origin,
           interest_group_owner_if_scoring_signals.CopyAsOptional()});

  // Return existing CompressionGroupData if there's already a matching
  // compression group.
  if (!new_element_created) {
    return scoped_refptr<CompressionGroupData>(
        compression_group_it->second.compression_group_data);
  }

  // Create a CompressionGroupData if a new compression group was created.
  // `compression_group_id` is left as -1. One will be assigned when the request
  // is sent over the wire.
  scoped_refptr<CompressionGroupData> compression_group_data =
      base::MakeRefCounted<CompressionGroupData>(
          this,
          ReceiverRestrictions{fetch_key.signals_type(),
                               fetch_key.script_origin()},
          fetch_it, compression_group_it);
  compression_group_it->second.compression_group_data =
      compression_group_data.get();
  compression_group_data_map_.emplace(
      compression_group_data->compression_group_token(),
      compression_group_data.get());
  return compression_group_data;
}

void TrustedSignalsCacheImpl::GetTrustedSignals(
    const base::UnguessableToken& compression_group_token,
    mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCacheClient>
        client) {
  auto compression_group_data_it =
      compression_group_data_map_.find(compression_group_token);
  // This can racily happen if a an auction is cancelled, so silently ignore
  // unrecognized IDs. This can also happen if a random ID is arbitrarily
  // requested, but the error message is for the common case.
  if (compression_group_data_it == compression_group_data_map_.end()) {
    // An error message shouldn't make it back to the browser process if this
    // happens, but provide one just in case it unexpectedly does.
    SendNoLiveEntryErrorToClient(std::move(client));
    return;
  }

  CompressionGroupData* compression_group_data =
      compression_group_data_it->second;
  if (receiver_set_.current_context() !=
      compression_group_data->receiver_restrictions()) {
    receiver_set_.ReportBadMessage(
        "Data from wrong compression group requested.");
    return;
  }

  // If the fetch is still pending, add to the list of pending clients.
  if (!compression_group_data->has_data()) {
    compression_group_data->AddPendingClient(std::move(client));
    return;
  }

  // Otherwise, provide the cached data immediately, which will then also
  // destroy `client`.
  SendResultToClient(std::move(client), compression_group_data->data());
}

void TrustedSignalsCacheImpl::GetCoordinatorKey(FetchMap::iterator fetch_it) {
  // Fetch should not have started yet.
  DCHECK(!fetch_it->second.fetcher);
  // If all the compression groups were deleted, the Fetch should have been
  // destroyed.
  DCHECK(!fetch_it->second.compression_groups.empty());

  // Invoking the callback to get the key here instead of in the
  // TrustedSignalsFetcher allows new partitions to be added to the fetch while
  // retrieving the key, and means that the Fetcher doesn't need to cache the
  // request body, or the information needed to create it, while waiting for the
  // key to be received.
  get_coordinator_key_callback_.Run(
      url::Origin::Create(fetch_it->first.trusted_signals_url()),
      fetch_it->first.coordinator,
      base::BindOnce(&TrustedSignalsCacheImpl::OnCoordinatorKeyReceived,
                     fetch_it->second.weak_ptr_factory.GetWeakPtr(), fetch_it));
}

void TrustedSignalsCacheImpl::OnCoordinatorKeyReceived(
    FetchMap::iterator fetch_it,
    base::expected<BiddingAndAuctionServerKey, std::string>
        bidding_and_auction_server_key) {
  // Fetch should not have started yet.
  DCHECK(!fetch_it->second.fetcher);
  // If all the compression groups were deleted, the Fetch should have been
  // destroyed.
  DCHECK(!fetch_it->second.compression_groups.empty());

  // On failure, synchronously call OnFetchComplete(). This method may be called
  // re-entrantly from FetchCoordinatorKey(), but that's safe, since this class
  // doesn't report errors directly to the caller, so no need to worry about
  // issues with the caller tearing down objects in OnFetchComplete().
  if (!bidding_and_auction_server_key.has_value()) {
    OnFetchComplete(
        fetch_it,
        base::unexpected(std::move(bidding_and_auction_server_key).error()));
    return;
  }

  fetch_it->second.coordinator_key =
      std::move(bidding_and_auction_server_key).value();
  StartFetchIfReady(fetch_it);
}

void TrustedSignalsCacheImpl::SetFetchCanStart(FetchMap::iterator fetch_it) {
  // Nothing to do it already set.
  if (fetch_it->second.can_start) {
    return;
  }

  fetch_it->second.can_start = true;
  StartFetchIfReady(fetch_it);
}

void TrustedSignalsCacheImpl::StartFetchIfReady(FetchMap::iterator fetch_it) {
  // Fetch should not have been started yet.
  DCHECK(!fetch_it->second.fetcher);

  if (!fetch_it->second.can_start || !fetch_it->second.coordinator_key) {
    return;
  }

  if (fetch_it->first.signals_type() == SignalsType::kBidding) {
    StartBiddingSignalsFetch(fetch_it);
  } else {
    StartScoringSignalsFetch(fetch_it);
  }
}

void TrustedSignalsCacheImpl::StartBiddingSignalsFetch(
    FetchMap::iterator fetch_it) {
  std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
      bidding_partition_map;
  const FetchKey* fetch_key = &fetch_it->first;
  Fetch* fetch = &fetch_it->second;
  DCHECK(!fetch->fetcher);
  DCHECK(fetch->coordinator_key);
  DCHECK(fetch->can_start);

  fetch->fetcher = CreateFetcher();

  int next_compression_group_id = 0;
  for (auto& compression_group_pair : fetch->compression_groups) {
    auto* compression_group = &compression_group_pair.second;
    compression_group->compression_group_id = next_compression_group_id++;

    // Note that this will insert a new compression group.
    auto& bidding_partitions =
        bidding_partition_map[compression_group->compression_group_id];
    // The CompressionGroupData should only have bidding entries.
    DCHECK(compression_group->compression_group_data->scoring_cache_entries()
               .empty());
    for (const auto& cache_entry_it :
         compression_group->compression_group_data->bidding_cache_entries()) {
      auto* cache_entry = &cache_entry_it.second->second;
      auto* cache_key = &cache_entry_it.second->first;
      // Passing int all these pointers is safe, since `bidding_partitions` will
      // be destroyed at the end of this function, and FetchBiddingSignals()
      // will not retain pointers to them.
      bidding_partitions.emplace_back(
          cache_entry->partition_id, &cache_entry->interest_group_names,
          &cache_entry->keys, &cache_key->additional_params,
          cache_key->buyer_tkv_signals.has_value()
              ? &cache_key->buyer_tkv_signals.value()
              : nullptr);
    }
  }
  fetch->fetcher->FetchBiddingSignals(
      *data_decoder_manager_, fetch_key->url_loader_factory.get(),
      fetch_key->frame_tree_node_id, std::move(fetch->devtools_auction_ids),
      fetch_key->main_frame_origin, fetch_key->ip_address_space,
      GetNetworkPartitionNonce(fetch_key->network_partition_nonce_key),
      fetch_key->script_origin(), fetch_key->trusted_signals_url(),
      *fetch->coordinator_key, bidding_partition_map,
      base::BindOnce(&TrustedSignalsCacheImpl::OnFetchComplete,
                     base::Unretained(this), fetch_it));
}

void TrustedSignalsCacheImpl::StartScoringSignalsFetch(
    FetchMap::iterator fetch_it) {
  const FetchKey* fetch_key = &fetch_it->first;
  Fetch* fetch = &fetch_it->second;
  DCHECK(!fetch->fetcher);
  DCHECK(fetch->coordinator_key);
  DCHECK(fetch->can_start);

  fetch->fetcher = CreateFetcher();

  std::map<int, std::vector<TrustedSignalsFetcher::ScoringPartition>>
      scoring_partition_map;
  int next_compression_group_id = 0;
  for (auto& compression_group_pair : fetch->compression_groups) {
    auto* compression_group = &compression_group_pair.second;
    compression_group->compression_group_id = next_compression_group_id++;

    // Note that this will insert a new compression group.
    auto& scoring_partitions =
        scoring_partition_map[compression_group->compression_group_id];
    // The CompressionGroupData should only have scoring entries.
    DCHECK(compression_group->compression_group_data->bidding_cache_entries()
               .empty());
    for (const auto& cache_entry_it :
         compression_group->compression_group_data->scoring_cache_entries()) {
      auto* cache_entry = &cache_entry_it.second->second;
      auto* cache_key = &cache_entry_it.second->first;
      // Passing int all these pointers is safe, since `scoring_partitions` will
      // be destroyed at the end of this function, and FetchBiddingSignals()
      // will not retain pointers to them.
      scoring_partitions.emplace_back(
          cache_entry->partition_id, &cache_key->render_url,
          &cache_key->component_render_urls, &cache_key->additional_params,
          cache_key->seller_tkv_signals.has_value()
              ? &cache_key->seller_tkv_signals.value()
              : nullptr);
    }
  }
  fetch->fetcher->FetchScoringSignals(
      *data_decoder_manager_, fetch_key->url_loader_factory.get(),
      fetch_key->frame_tree_node_id, std::move(fetch->devtools_auction_ids),
      fetch_key->main_frame_origin, fetch_key->ip_address_space,
      GetNetworkPartitionNonce(fetch_key->network_partition_nonce_key),
      fetch_key->script_origin(), fetch_key->trusted_signals_url(),
      *fetch->coordinator_key, scoring_partition_map,
      base::BindOnce(&TrustedSignalsCacheImpl::OnFetchComplete,
                     base::Unretained(this), fetch_it));
  fetch->devtools_auction_ids.clear();
}

void TrustedSignalsCacheImpl::OnFetchComplete(
    FetchMap::iterator fetch_it,
    TrustedSignalsFetcher::SignalsFetchResult signals_fetch_result) {
  Fetch* fetch = &fetch_it->second;

  // If the result is not an error, separate out the data for each compression
  // group in the request, prior to sending the data to pending requests for it.
  // If any result is missing, replace `signals_fetch_result` with an error and
  // throw away all extracted data. In that case, the error will be used for all
  // compression groups, even those that did receive data.
  std::vector<std::pair<CompressionGroupData*, CachedResult>>
      compression_group_results;
  if (signals_fetch_result.has_value()) {
    compression_group_results.reserve(fetch->compression_groups.size());
    for (auto& compression_group_pair : fetch->compression_groups) {
      Fetch::CompressionGroup* compression_group =
          &compression_group_pair.second;
      CachedResult result;
      auto signals_fetch_result_it =
          signals_fetch_result->find(compression_group->compression_group_id);
      if (signals_fetch_result_it == signals_fetch_result->end()) {
        // If this happens, all results previously moved into
        // `compression_group_results` will be ignored. Clearing this is not
        // strictly necessary, but is done out of caution.
        compression_group_results.clear();

        signals_fetch_result = base::unexpected(
            base::StringPrintf("Fetched signals missing compression group %i.",
                               compression_group->compression_group_id));
        break;
      }
      result = std::move(signals_fetch_result_it->second);
      compression_group_results.emplace_back(
          compression_group->compression_group_data, std::move(result));
    }
  }

  if (signals_fetch_result.has_value()) {
    // On success, pass each CachedData gathered in the earlier loop to each
    // CompressionGroupData.

    // All compression groups should have been found and have their results
    // added to `compression_group_results` in the previous loop.
    CHECK_EQ(compression_group_results.size(),
             fetch->compression_groups.size());

    for (auto& compression_group_result : compression_group_results) {
      compression_group_result.first->SetData(
          std::move(compression_group_result.second));
      // Update `size_`, now that SetData() caused in the compression group's
      // size to be calculated.
      size_ += compression_group_result.first->size();
    }
  } else {
    // On error, copy the shared error value to each group's
    // CompressionGroupData.
    for (auto& compression_group_pair : fetch->compression_groups) {
      CompressionGroupData* compression_group =
          compression_group_pair.second.compression_group_data;
      compression_group->SetData(
          base::unexpected(signals_fetch_result.error()));
      // On error, the size of compression groups is treated as zero, as they'll
      // be discarded as soon as they fall out of use.
      DCHECK_EQ(compression_group->size(), 0u);
    }
  }

  // While the cache is too large, remove entries from `lru_cache_`, if there
  // are any. May not get below max cache size, due to not being able to delete
  // live entries.
  while (size_ > kMaxCacheSizeBytes && !lru_list_.empty()) {
    RemoveFromLruList(lru_list_.begin());
  }

  // The SetData() calls above cleared the references to the fetch held by the
  // CompressionGroupData, so it's now safe to remove.
  fetches_.erase(fetch_it);
}

void TrustedSignalsCacheImpl::OnLastHandleDestroyed(
    scoped_refptr<CompressionGroupData> compression_group_data) {
  // If the maximum size has already been exceeded, don't add the entry to the
  // LruList.
  if (size_ > kMaxCacheSizeBytes) {
    // In this case, `lru_list_` should have no entries in it, since only
    // entries with live Handles can exceed the limit, and entries in the list
    // should not have any live Handles.
    DCHECK(lru_list_.empty());
    return;
  }

  // If the entry is still loading, or can't be used by new requests, due to
  // expiration or having been removed from the index, discard it.
  if (!compression_group_data->IsLoadedAndCanBeUsedByNewRequests()) {
    return;
  }

  // Add entry to the LruList. This is the inverse of RemoveFromLruList(),
  // below.
  auto it =
      lru_list_.emplace(lru_list_.end(), std::move(compression_group_data));
  (*it)->SetLruListIt(it);
  MaybeStartCleanupTimer();
}

void TrustedSignalsCacheImpl::RemoveFromLruList(LruList::iterator lru_list_it) {
  // This needs to be done before removing the CompressionGroupData from the
  // LruList, as removal may destroy the CompressionGroupData.
  (*lru_list_it)->ClearLruListIt();
  lru_list_.erase(lru_list_it);

  // Stop the timer if it's no longer needed. This makes timeouts a easier to
  // reason about for testing.
  if (lru_list_.empty()) {
    cleanup_timer_.Stop();
  }
}

void TrustedSignalsCacheImpl::OnCompressionGroupDataDestroyed(
    CompressionGroupData& compression_group_data) {
  DCHECK_LE(compression_group_data.size(), size_);
  size_ -= compression_group_data.size();

  // Need to clean up the *CacheEntries associated with the
  // CompressionGroupData.
  for (auto cache_entry_it : compression_group_data.bidding_cache_entries()) {
    bidding_cache_entries_.erase(cache_entry_it.second);
  }
  for (auto cache_entry_it : compression_group_data.scoring_cache_entries()) {
    scoring_cache_entries_.erase(cache_entry_it.second);
  }

  // If `compression_group_data` has a fetch, started or not, need to update the
  // fetch and send an error to any Mojo clients waiting on the
  // CompressionGroupData.
  if (!compression_group_data.has_data()) {
    Fetch* fetch = &compression_group_data.fetch()->second;

    DCHECK_EQ(compression_group_data.fetch_compression_group()
                  ->second.compression_group_data,
              &compression_group_data);

    // Erase the compression group from the fetch. If the request hasn't yet
    // started, the group won't be requested. If it has started, any response
    // for the (now unknown) compression group will be discarded.
    fetch->compression_groups.erase(
        compression_group_data.fetch_compression_group());

    // Abort the fetch, if it has no remaining compression groups.
    if (fetch->compression_groups.empty()) {
      fetches_.erase(compression_group_data.fetch());
    }

    // Inform all pending clients waiting on the CompressionGroupData that the
    // request was cancelled.
    auto pending_clients = compression_group_data.TakePendingClients();
    for (auto& pending_client : pending_clients) {
      SendNoLiveEntryErrorToClient(std::move(pending_client));
    }
  }

  compression_group_data_map_.erase(
      compression_group_data.compression_group_token());
}

void TrustedSignalsCacheImpl::DestroyBiddingCacheEntry(
    BiddingCacheEntryMap::iterator cache_entry_it) {
  CompressionGroupData* compression_group_data =
      cache_entry_it->second.compression_group_data;
  // The compression group's fetch must either have completed, or its Fetch must
  // have already started.
  CHECK(compression_group_data->has_data() ||
        compression_group_data->fetch()->second.fetcher);
  compression_group_data->RemoveBiddingCacheEntry(&cache_entry_it->second);
  bidding_cache_entries_.erase(cache_entry_it);
}

void TrustedSignalsCacheImpl::DestroyScoringCacheEntry(
    ScoringCacheEntryMap::iterator cache_entry_it) {
  CompressionGroupData* compression_group_data =
      cache_entry_it->second.compression_group_data;
  // The compression group's fetch must either have completed, or its Fetch must
  // have already started.
  CHECK(compression_group_data->has_data() ||
        compression_group_data->fetch()->second.fetcher);
  compression_group_data->RemoveScoringCacheEntry(&cache_entry_it->second);
  scoring_cache_entries_.erase(cache_entry_it);
}

base::UnguessableToken TrustedSignalsCacheImpl::GetNetworkPartitionNonce(
    const NetworkPartitionNonceKey& network_partition_nonce_key) {
  auto it = network_partition_nonce_cache_.Get(network_partition_nonce_key);
  if (it == network_partition_nonce_cache_.end()) {
    it = network_partition_nonce_cache_.Put(network_partition_nonce_key,
                                            base::UnguessableToken::Create());
  }
  return it->second;
}

void TrustedSignalsCacheImpl::MaybeStartCleanupTimer() {
  if (cleanup_timer_.IsRunning() || lru_list_.empty()) {
    return;
  }

  // Unretained is safe here because `cleanup_timer_` won't run tasks after it
  // has been destroyed, and `this` owns `cleanup_timer_`.
  cleanup_timer_.Start(FROM_HERE,
                       (*lru_list_.begin())->scheduled_cleanup_time() -
                           base::TimeTicks::Now() + kCleanupInterval,
                       base::BindOnce(&TrustedSignalsCacheImpl::Cleanup,
                                      base::Unretained(this)));
}

void TrustedSignalsCacheImpl::Cleanup() {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!lru_list_.empty() &&
         (*lru_list_.begin())->scheduled_cleanup_time() <= now) {
    RemoveFromLruList(lru_list_.begin());
  }
  MaybeStartCleanupTimer();
}

std::unique_ptr<TrustedSignalsFetcher>
TrustedSignalsCacheImpl::CreateFetcher() {
  return std::make_unique<TrustedSignalsFetcher>();
}

}  // namespace content
