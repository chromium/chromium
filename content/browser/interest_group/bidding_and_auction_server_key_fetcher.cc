// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include "base/base64.h"
#include "base/rand_util.h"
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

BiddingAndAuctionServerKeyFetcher::BiddingAndAuctionServerKeyFetcher() {
  fetcher_state_map_.insert_or_assign(blink::mojom::AdAuctionCoordinator::kGCP,
                                      PerCoordinatorFetcherState());
  fetcher_state_map_.insert_or_assign(blink::mojom::AdAuctionCoordinator::kAWS,
                                      PerCoordinatorFetcherState());
  CHECK_EQ(
      fetcher_state_map_.size(),
      static_cast<size_t>(blink::mojom::AdAuctionCoordinator::kMaxValue) -
          static_cast<size_t>(blink::mojom::AdAuctionCoordinator::kMinValue) +
          1);
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeBiddingAndAuctionServer)) {
    fetcher_state_map_.at(blink::mojom::AdAuctionCoordinator::kGCP).key_url =
        GURL(blink::features::kFledgeBiddingAndAuctionKeyURL.Get());
  }
}

BiddingAndAuctionServerKeyFetcher::~BiddingAndAuctionServerKeyFetcher() =
    default;

void BiddingAndAuctionServerKeyFetcher::GetOrFetchKey(
    network::mojom::URLLoaderFactory* loader_factory,
    blink::mojom::AdAuctionCoordinator coordinator,
    BiddingAndAuctionServerKeyFetcherCallback callback) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  if (!state.key_url.is_valid()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // If we have keys and they haven't expired just call the callback now.
  if (state.keys.size() > 0 && state.expiration > base::Time::Now()) {
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    std::move(callback).Run(
        state.keys[base::RandInt(0, state.keys.size() - 1)]);
    return;
  }

  state.queue.push_back(std::move(callback));
  if (state.queue.size() > 1) {
    return;
  }
  state.keys.clear();

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
      loader_factory,
      base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnFetchKeyComplete,
                     weak_ptr_factory_.GetWeakPtr(), coordinator),
      /*max_body_size=*/kMaxBodySize);
}

void BiddingAndAuctionServerKeyFetcher::OnFetchKeyComplete(
    blink::mojom::AdAuctionCoordinator coordinator,
    std::unique_ptr<std::string> response) {
  fetcher_state_map_.at(coordinator).loader.reset();
  if (!response) {
    FailAllCallbacks(coordinator);
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnParsedKeys,
                     weak_ptr_factory_.GetWeakPtr(), coordinator));
}

void BiddingAndAuctionServerKeyFetcher::OnParsedKeys(
    blink::mojom::AdAuctionCoordinator coordinator,
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

  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  state.keys = std::move(keys);
  state.expiration = base::Time::Now() + kKeyRequestInterval;

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
    blink::mojom::AdAuctionCoordinator coordinator) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  while (!state.queue.empty()) {
    // We call the callback *before* removing the current request from the list.
    // That avoids the problem if callback were to synchronously enqueue another
    // request. If we removed the current request first then enqueued the
    // request, that would start another thread of execution since there was an
    // empty queue.
    std::move(state.queue.front()).Run(absl::nullopt);
    state.queue.pop_front();
  }
}

}  // namespace content
