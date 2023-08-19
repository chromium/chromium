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

BiddingAndAuctionServerKeyFetcher::BiddingAndAuctionServerKeyFetcher() =
    default;
BiddingAndAuctionServerKeyFetcher::~BiddingAndAuctionServerKeyFetcher() =
    default;

void BiddingAndAuctionServerKeyFetcher::GetOrFetchKey(
    network::mojom::URLLoaderFactory* loader_factory,
    BiddingAndAuctionServerKeyFetcherCallback callback) {
  GURL key_url(blink::features::kFledgeBiddingAndAuctionKeyURL.Get());
  if (!key_url.is_valid()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // If we have keys and they haven't expired just call the callback now.
  if (keys_.size() > 0 && expiration_ > base::Time::Now()) {
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    std::move(callback).Run(keys_[base::RandInt(0, keys_.size() - 1)]);
    return;
  }

  queue_.push_back(std::move(callback));
  if (queue_.size() > 1) {
    return;
  }
  keys_.clear();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = key_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->trusted_params.emplace();
  resource_request->trusted_params->isolation_info = net::IsolationInfo();
  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      kBiddingAndAuctionServerKeyFetchTrafficAnnotation);
  loader_->SetTimeoutDuration(kRequestTimeout);

  loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnFetchKeyComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      /*max_body_size=*/kMaxBodySize);
}

void BiddingAndAuctionServerKeyFetcher::OnFetchKeyComplete(
    std::unique_ptr<std::string> response) {
  if (!response) {
    FailAllCallbacks();
    return;
  }
  loader_.reset();
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnParsedKeys,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BiddingAndAuctionServerKeyFetcher::OnParsedKeys(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    FailAllCallbacks();
    return;
  }

  const base::Value::Dict* response_dict = result->GetIfDict();
  if (!response_dict) {
    FailAllCallbacks();
    return;
  }

  const base::Value::List* keys = response_dict->FindList("keys");
  if (!keys) {
    FailAllCallbacks();
    return;
  }

  for (const auto& entry : *keys) {
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
    keys_.push_back(std::move(key));
  }

  if (keys_.size() == 0) {
    FailAllCallbacks();
    return;
  }

  expiration_ = base::Time::Now() + kKeyRequestInterval;

  while (!queue_.empty()) {
    // We call the callback *before* removing the current request from the list.
    // It is possible that the callback may synchronously enqueue another
    // request. If we remove the current request first then enqueuing the
    // request would start another thread of execution since there was an empty
    // queue.
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    std::move(queue_.front()).Run(keys_[base::RandInt(0, keys_.size() - 1)]);
    queue_.pop_front();
  }
}

void BiddingAndAuctionServerKeyFetcher::FailAllCallbacks() {
  while (!queue_.empty()) {
    // We call the callback *before* removing the current request from the list.
    // It is possible that the callback may synchronously enqueue another
    // request. If we remove the current request first then enqueuing the
    // request would start another thread of execution since there was an empty
    // queue.
    std::move(queue_.front()).Run(absl::nullopt);
    queue_.pop_front();
  }
}

}  // namespace content
