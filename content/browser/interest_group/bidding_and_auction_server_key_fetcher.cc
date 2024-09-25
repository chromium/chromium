// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
constexpr base::TimeDelta kKeyRequestInterval = base::Days(7);
constexpr base::TimeDelta kRequestTimeout = base::Seconds(5);
const size_t kMaxBodySize = 2048;

constexpr net::NetworkTrafficAnnotationTag
    kBiddingAndAuctionServerKeyFetchTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "bidding_and_auction_server_key_fetch",
            R"(
    semantics {
      sender: "Chrome Bidding and Auction Server Key Fetch"
      last_reviewed: "2023-06-05"
      description:
        "Request to the Bidding and Auction Server keystore to fetch the "
        "public key which will be used to encrypt the request payload sent "
        "to the trusted auction server."
      trigger:
        "Start of a Protected Audience Bidding and Server auction"
      data:
        "No data is sent with this request."
      user_data {
        type: NONE
      }
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "privacy-sandbox-dev@chromium.org"
        }
      }
    }
    policy {
      cookies_allowed: NO
      setting:
        "Disable the Protected Audiences API."
      chrome_policy {
      }
    }
    comments:
      ""
    )");
}  // namespace

BiddingAndAuctionServerKeyFetcher::PerCoordinatorFetcherState::
    PerCoordinatorFetcherState() = default;
BiddingAndAuctionServerKeyFetcher::PerCoordinatorFetcherState::
    ~PerCoordinatorFetcherState() = default;
BiddingAndAuctionServerKeyFetcher::PerCoordinatorFetcherState::
    PerCoordinatorFetcherState(PerCoordinatorFetcherState&& state) = default;
BiddingAndAuctionServerKeyFetcher::PerCoordinatorFetcherState&
BiddingAndAuctionServerKeyFetcher::PerCoordinatorFetcherState::operator=(
    PerCoordinatorFetcherState&& state) = default;

BiddingAndAuctionServerKeyFetcher::BiddingAndAuctionServerKeyFetcher(
    InterestGroupManagerImpl* manager,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : loader_factory_(std::move(loader_factory)), manager_(manager) {
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeBiddingAndAuctionServer)) {
    std::string config =
        blink::features::kFledgeBiddingAndAuctionKeyConfig.Get();
    if (!config.empty()) {
      std::optional<base::Value> config_value = base::JSONReader::Read(config);
      if (config_value && config_value->is_dict()) {
        for (const auto kv : config_value->GetDict()) {
          if (!kv.second.is_string()) {
            continue;
          }
          url::Origin coordinator = url::Origin::Create(GURL(kv.first));
          if (coordinator.scheme() != url::kHttpsScheme) {
            continue;
          }

          PerCoordinatorFetcherState state;
          state.key_url = GURL(kv.second.GetString());
          fetcher_state_map_.insert_or_assign(std::move(coordinator),
                                              std::move(state));
        }
      }
    }
    GURL key_url = GURL(blink::features::kFledgeBiddingAndAuctionKeyURL.Get());
    if (key_url.is_valid()) {
      PerCoordinatorFetcherState state;
      state.key_url = std::move(key_url);
      fetcher_state_map_.insert_or_assign(default_gcp_coordinator_,
                                          std::move(state));
    }
  }
}

BiddingAndAuctionServerKeyFetcher::~BiddingAndAuctionServerKeyFetcher() =
    default;

void BiddingAndAuctionServerKeyFetcher::MaybePrefetchKeys() {
  // We only prefetch keys if the prefetching is enabled and if
  // kFledgeBiddingAndAuctionServer is enabled. We don't need to check
  // kFledgeBiddingAndAuctionServer because if it's not enabled
  // fetcher_state_map_ would have no keys.
  if (!base::FeatureList::IsEnabled(features::kFledgePrefetchBandAKeys)) {
    return;
  }
  // We only want to prefetch once.
  if (did_prefetch_keys_) {
    return;
  }
  did_prefetch_keys_ = true;
  for (auto& [coordinator, state] : fetcher_state_map_) {
    if (state.keys.size() == 0 || state.expiration < base::Time::Now()) {
      FetchKeys(coordinator, state, base::DoNothing());
    }
  }
}

void BiddingAndAuctionServerKeyFetcher::GetOrFetchKey(
    const std::optional<url::Origin>& maybe_coordinator,
    BiddingAndAuctionServerKeyFetcherCallback callback) {
  const url::Origin& coordinator = maybe_coordinator.has_value()
                                       ? *maybe_coordinator
                                       : default_gcp_coordinator_;
  auto it = fetcher_state_map_.find(coordinator);
  if (it == fetcher_state_map_.end()) {
    std::move(callback).Run(
        base::unexpected<std::string>("Invalid Coordinator"));
    return;
  }
  PerCoordinatorFetcherState& state = it->second;
  if (!state.key_url.is_valid()) {
    std::move(callback).Run(
        base::unexpected<std::string>("Invalid Coordinator"));
    return;
  }

  // If we have keys and they haven't expired just call the callback now.
  if (state.keys.size() > 0 && state.expiration > base::Time::Now()) {
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    base::UmaHistogramBoolean("Ads.InterestGroup.ServerAuction.KeyFetch.Cached",
                              true);
    std::move(callback).Run(
        state.keys[base::RandInt(0, state.keys.size() - 1)]);
    return;
  }
  base::UmaHistogramBoolean("Ads.InterestGroup.ServerAuction.KeyFetch.Cached",
                            false);
  FetchKeys(coordinator, state, std::move(callback));
}

void BiddingAndAuctionServerKeyFetcher::FetchKeys(
    const url::Origin& coordinator,
    PerCoordinatorFetcherState& state,
    BiddingAndAuctionServerKeyFetcherCallback callback) {
  state.fetch_start = base::TimeTicks::Now();
  state.queue.push_back(std::move(callback));
  if (state.queue.size() > 1) {
    return;
  }

  state.keys.clear();

  if (base::FeatureList::IsEnabled(features::kFledgeStoreBandAKeysInDB)) {
    manager_->GetBiddingAndAuctionServerKeys(
        coordinator,
        base::BindOnce(
            &BiddingAndAuctionServerKeyFetcher::OnFetchKeysFromDatabaseComplete,
            weak_ptr_factory_.GetWeakPtr(), coordinator));
  } else {
    FetchKeysFromNetwork(coordinator);
  }
}

void BiddingAndAuctionServerKeyFetcher::OnFetchKeysFromDatabaseComplete(
    const url::Origin& coordinator,
    std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>
        expiration_and_keys) {
  if (expiration_and_keys.second.empty() ||
      expiration_and_keys.first < base::Time::Now()) {
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ServerAuction.KeyFetch.DBCached", false);
    FetchKeysFromNetwork(coordinator);
  } else {
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ServerAuction.KeyFetch.DBCached", true);
    CacheKeysAndRunAllCallbacks(coordinator, expiration_and_keys.second,
                                expiration_and_keys.first);
  }
}

void BiddingAndAuctionServerKeyFetcher::FetchKeysFromNetwork(
    const url::Origin& coordinator) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  state.network_fetch_start = base::TimeTicks::Now();

  CHECK(!state.loader);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = state.key_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = net::IsolationInfo();
  state.loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      kBiddingAndAuctionServerKeyFetchTrafficAnnotation);
  state.loader->SetTimeoutDuration(kRequestTimeout);

  state.loader->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(
          &BiddingAndAuctionServerKeyFetcher::OnFetchKeysFromNetworkComplete,
          weak_ptr_factory_.GetWeakPtr(), coordinator),
      /*max_body_size=*/kMaxBodySize);
}

void BiddingAndAuctionServerKeyFetcher::OnFetchKeysFromNetworkComplete(
    url::Origin coordinator,
    std::unique_ptr<std::string> response) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  bool was_cached = state.loader->LoadedFromCache();
  state.loader.reset();
  if (!response) {
    FailAllCallbacks(coordinator);
    return;
  }
  base::UmaHistogramTimes(
      "Ads.InterestGroup.ServerAuction.KeyFetch.NetworkTime",
      base::TimeTicks::Now() - state.network_fetch_start);
  base::UmaHistogramBoolean(
      "Ads.InterestGroup.ServerAuction.KeyFetch.NetworkCached", was_cached);
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnParsedKeys,
                     weak_ptr_factory_.GetWeakPtr(), coordinator));
}

void BiddingAndAuctionServerKeyFetcher::OnParsedKeys(
    url::Origin coordinator,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    FailAllCallbacks(coordinator);
    return;
  }

  const base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    FailAllCallbacks(coordinator);
    return;
  }

  const base::Value::List* key_values = response_dict->FindList("keys");
  if (!key_values) {
    FailAllCallbacks(coordinator);
    return;
  }

  std::vector<BiddingAndAuctionServerKey> keys;
  for (const auto& entry : *key_values) {
    BiddingAndAuctionServerKey key;
    const base::Value::Dict* key_dict = entry.GetIfDict();
    if (!key_dict) {
      continue;
    }
    const std::string* key_value = key_dict->FindString("key");
    if (!key_value) {
      continue;
    }
    if (!base::Base64Decode(*key_value, &key.key)) {
      continue;
    }
    const std::string* id_value = key_dict->FindString("id");
    unsigned int key_id;
    if (!id_value || id_value->size() == 0 ||
        !base::HexStringToUInt(id_value->substr(0, 2), &key_id) ||
        key_id > 0xFF) {
      continue;
    }
    key.id = key_id;
    keys.push_back(std::move(key));
  }

  if (keys.size() == 0) {
    FailAllCallbacks(coordinator);
    return;
  }

  base::Time expiration = base::Time::Now() + kKeyRequestInterval;
  CacheKeysAndRunAllCallbacks(coordinator, keys, expiration);
  if (base::FeatureList::IsEnabled(features::kFledgeStoreBandAKeysInDB)) {
    manager_->SetBiddingAndAuctionServerKeys(coordinator, std::move(keys),
                                             expiration);
  }
}

void BiddingAndAuctionServerKeyFetcher::CacheKeysAndRunAllCallbacks(
    const url::Origin& coordinator,
    const std::vector<BiddingAndAuctionServerKey>& keys,
    base::Time expiration) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  state.keys = keys;
  state.expiration = expiration;
  base::UmaHistogramTimes("Ads.InterestGroup.ServerAuction.KeyFetch.TotalTime2",
                          base::TimeTicks::Now() - state.fetch_start);

  while (!state.queue.empty()) {
    // We call the callback *before* removing the current request from the list.
    // That avoids the problem if callback were to synchronously enqueue another
    // request. If we removed the current request first then enqueued the
    // request, that would start another thread of execution since there was an
    // empty queue.
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    std::move(state.queue.front())
        .Run(state.keys[base::RandInt(0, state.keys.size() - 1)]);
    state.queue.pop_front();
  }
}

void BiddingAndAuctionServerKeyFetcher::FailAllCallbacks(
    url::Origin coordinator) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  while (!state.queue.empty()) {
    // We call the callback *before* removing the current request from the list.
    // That avoids the problem if callback were to synchronously enqueue another
    // request. If we removed the current request first then enqueued the
    // request, that would start another thread of execution since there was an
    // empty queue.
    std::move(state.queue.front())
        .Run(base::unexpected<std::string>("Key fetch failed"));
    state.queue.pop_front();
  }
}

}  // namespace content
