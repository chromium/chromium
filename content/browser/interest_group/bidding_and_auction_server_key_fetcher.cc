// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.pb.h"
#include "content/browser/interest_group/interest_group_storage_metric_types.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/origin.h"

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

const struct {
  const char* origin;
  const char* key_url;
} kDefaultKeys[] = {
    {kDefaultBiddingAndAuctionGCPCoordinatorOrigin,
     kBiddingAndAuctionGCPCoordinatorKeyURL},
    {kBiddingAndAuctionGCPCoordinatorOrigin,
     kBiddingAndAuctionGCPCoordinatorKeyURL},
    {kBiddingAndAuctionAWSCoordinatorOrigin,
     kBiddingAndAuctionAWSCoordinatorKeyURL},
};

std::vector<BiddingAndAuctionServerKey> ParseKeysList(
    const base::Value::List* keys_list) {
  std::vector<BiddingAndAuctionServerKey> keys;
  for (const auto& entry : *keys_list) {
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
    // Previous versions depend on the first two characters being hex digits.
    if (!id_value || id_value->size() < 2 ||
        !base::IsHexDigit((*id_value)[0]) ||
        !base::IsHexDigit((*id_value)[1])) {
      continue;
    }
    key.id = *id_value;
    keys.push_back(std::move(key));
  }
  return keys;
}

std::vector<BiddingAndAuctionServerKey> KeysFromProto(
    google::protobuf::RepeatedPtrField<
        BiddingAndAuctionServerKeysProtos_BiddingAndAuctionServerKeyProto>&
        keys_proto) {
  std::vector<BiddingAndAuctionServerKey> keys;
  keys.reserve(keys_proto.size());
  for (auto& key_proto : keys_proto) {
    if (key_proto.id_str().empty()) {
      std::string id_str;
      base::AppendHexEncodedByte(key_proto.id(), id_str);
      keys.emplace_back(std::move(*key_proto.mutable_key()), std::move(id_str));
    } else {
      keys.emplace_back(std::move(*key_proto.mutable_key()),
                        std::move(*key_proto.mutable_id_str()));
    }
  }
  return keys;
}

void PopulateProtoFromKeys(
    const std::vector<BiddingAndAuctionServerKey>& keys,
    google::protobuf::RepeatedPtrField<
        BiddingAndAuctionServerKeysProtos_BiddingAndAuctionServerKeyProto>&
        proto_keys_out) {
  for (const auto& key : keys) {
    BiddingAndAuctionServerKeysProtos_BiddingAndAuctionServerKeyProto*
        key_proto = proto_keys_out.Add();
    uint32_t key_id = 0;
    bool success =
        base::HexStringToUInt(std::string_view(key.id).substr(0, 2), &key_id);
    DCHECK(success);
    key_proto->set_key(key.key);
    key_proto->set_id(key_id);
    key_proto->set_id_str(key.id);
  }
}

}  // namespace

BiddingAndAuctionKeySet::BiddingAndAuctionKeySet(
    std::vector<BiddingAndAuctionServerKey> keys)
    : keys_(std::move(keys)) {}
BiddingAndAuctionKeySet::BiddingAndAuctionKeySet(
    base::flat_map<url::Origin, std::vector<BiddingAndAuctionServerKey>>
        origin_scoped_keys)
    : origin_scoped_keys_(std::move(origin_scoped_keys)) {}
BiddingAndAuctionKeySet::~BiddingAndAuctionKeySet() = default;

BiddingAndAuctionKeySet::BiddingAndAuctionKeySet(
    BiddingAndAuctionKeySet&& keyset) = default;
BiddingAndAuctionKeySet& BiddingAndAuctionKeySet::operator=(
    BiddingAndAuctionKeySet&& keyset) = default;

bool BiddingAndAuctionKeySet::HasKeys() const {
  if (!keys_.empty() || !origin_scoped_keys_.empty()) {
    return true;
  }
  return false;
}

uint8_t BiddingAndAuctionKeySet::SchemaVersion() const {
  if (!origin_scoped_keys_.empty()) {
    return 2;
  }
  if (!keys_.empty()) {
    return 1;
  }
  return 0;
}

std::optional<BiddingAndAuctionServerKey> BiddingAndAuctionKeySet::GetRandomKey(
    const url::Origin& scoped_origin) const {
  if (!origin_scoped_keys_.empty()) {
    auto it = origin_scoped_keys_.find(scoped_origin);
    if (it == origin_scoped_keys_.end() || it->second.empty()) {
      return std::nullopt;
    }
    return it->second[base::RandInt(0, it->second.size() - 1)];
  } else if (!keys_.empty()) {
    return keys_[base::RandInt(0, keys_.size() - 1)];
  }
  return std::nullopt;
}

std::string BiddingAndAuctionKeySet::AsBinaryProto() const {
  BiddingAndAuctionServerKeysProtos keys_protos;
  PopulateProtoFromKeys(keys_, *keys_protos.mutable_keys());
  for (const auto& [origin, keys] : origin_scoped_keys_) {
    BiddingAndAuctionServerKeysProtos_BiddingAndAuctionServerKeyProtos
        key_protos;
    PopulateProtoFromKeys(keys, *key_protos.mutable_keys());
    keys_protos.mutable_origin_scoped_keys()->insert(
        {origin.Serialize(), std::move(key_protos)});
  }
  std::string key_protos_str;
  if (!keys_protos.SerializeToString(&key_protos_str)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult."
        "BiddingAndAuctionServerKeyProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    return "";
  }
  base::UmaHistogramEnumeration(
      "Storage.InterestGroup.ProtoSerializationResult."
      "BiddingAndAuctionServerKeyProtos",
      InterestGroupStorageProtoSerializationResult::kSucceeded);
  return key_protos_str;
}

/*static*/ std::optional<BiddingAndAuctionKeySet>
BiddingAndAuctionKeySet::FromBinaryProto(std::string key_blob) {
  BiddingAndAuctionServerKeysProtos keys_protos;
  bool success = keys_protos.ParseFromString(key_blob);
  if (!success) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult."
        "BiddingAndAuctionServerKeyProtos",
        InterestGroupStorageProtoDeserializationResult::kFailed);
    return std::nullopt;
  }
  base::UmaHistogramEnumeration(
      "Storage.InterestGroup.ProtoDeserializationResult."
      "BiddingAndAuctionServerKeyProtos",
      InterestGroupStorageProtoDeserializationResult::kSucceeded);

  if (!keys_protos.origin_scoped_keys().empty()) {
    std::vector<std::pair<url::Origin, std::vector<BiddingAndAuctionServerKey>>>
        origin_scoped_keys;
    for (auto& [origin_str, key_protos] :
         *keys_protos.mutable_origin_scoped_keys()) {
      url::Origin origin = url::Origin::Create(GURL(origin_str));
      if (origin.opaque()) {
        // If we can't recover the key list, just return nullopt so we fetch it
        // again.
        return std::nullopt;
      }

      std::vector<BiddingAndAuctionServerKey> keys =
          KeysFromProto(*key_protos.mutable_keys());
      origin_scoped_keys.emplace_back(origin, std::move(keys));
    }
    return BiddingAndAuctionKeySet(std::move(origin_scoped_keys));
  } else {
    if (keys_protos.keys().empty()) {
      // No keys in this proto. Return nullopt so we fetch it again.
      return std::nullopt;
    }
    std::vector<BiddingAndAuctionServerKey> keys =
        KeysFromProto(*keys_protos.mutable_keys());
    return BiddingAndAuctionKeySet(std::move(keys));
  }
}

BiddingAndAuctionServerKeyFetcher::CallbackQueueItem::CallbackQueueItem(
    BiddingAndAuctionServerKeyFetcherCallback callback,
    url::Origin scope_origin)
    : callback(std::move(callback)), scope_origin(std::move(scope_origin)) {}
BiddingAndAuctionServerKeyFetcher::CallbackQueueItem::~CallbackQueueItem() =
    default;
BiddingAndAuctionServerKeyFetcher::CallbackQueueItem::CallbackQueueItem(
    CallbackQueueItem&& item) = default;
BiddingAndAuctionServerKeyFetcher::CallbackQueueItem&
BiddingAndAuctionServerKeyFetcher::CallbackQueueItem::operator=(
    CallbackQueueItem&& item) = default;

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
  for (const auto& key_config : kDefaultKeys) {
    url::Origin coordinator = url::Origin::Create(GURL(key_config.origin));
    DCHECK_EQ(coordinator.scheme(), url::kHttpsScheme);
    PerCoordinatorFetcherState state;
    state.key_url = GURL(key_config.key_url);
    state.version = 1;
    state.apis.Put(TrustedServerAPIType::kBiddingAndAuction);
    state.apis.Put(TrustedServerAPIType::kTrustedKeyValue);
    if (!state.key_url.is_valid()) {
      continue;
    }
    fetcher_state_map_.insert_or_assign(std::move(coordinator),
                                        std::move(state));
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeBiddingAndAuctionServer)) {
    std::string config =
        blink::features::kFledgeBiddingAndAuctionKeyConfig.Get();
    if (!config.empty()) {
      std::optional<base::Value> config_value =
          base::JSONReader::Read(config, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
          state.version = 1;
          state.apis.Put(TrustedServerAPIType::kBiddingAndAuction);
          state.apis.Put(TrustedServerAPIType::kTrustedKeyValue);
          if (!state.key_url.is_valid()) {
            fetcher_state_map_.erase(coordinator);
            continue;
          }
          fetcher_state_map_.insert_or_assign(std::move(coordinator),
                                              std::move(state));
        }
      }
    }
    GURL key_url = GURL(blink::features::kFledgeBiddingAndAuctionKeyURL.Get());
    if (key_url.is_valid()) {
      PerCoordinatorFetcherState state;
      state.key_url = std::move(key_url);
      state.version = 1;
      state.apis.Put(TrustedServerAPIType::kBiddingAndAuction);
      state.apis.Put(TrustedServerAPIType::kTrustedKeyValue);
      fetcher_state_map_.insert_or_assign(default_gcp_coordinator_,
                                          std::move(state));
    }
  }
  if (base::FeatureList::IsEnabled(blink::features::kFledgeOriginScopedKeys)) {
    std::string config = blink::features::kFledgeOriginScopedKeyConfig.Get();
    if (!config.empty()) {
      std::optional<base::Value> config_value =
          base::JSONReader::Read(config, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
          state.version = 2;
          state.apis.Put(TrustedServerAPIType::kBiddingAndAuction);
          state.apis.Put(TrustedServerAPIType::kTrustedKeyValue);
          if (!state.key_url.is_valid()) {
            fetcher_state_map_.erase(coordinator);
            continue;
          }
          fetcher_state_map_.insert_or_assign(std::move(coordinator),
                                              std::move(state));
        }
      }
    }
  }
}

BiddingAndAuctionServerKeyFetcher::~BiddingAndAuctionServerKeyFetcher() =
    default;

void BiddingAndAuctionServerKeyFetcher::MaybePrefetchKeys() {
  // We only want to prefetch once.
  if (did_prefetch_keys_) {
    return;
  }
  did_prefetch_keys_ = true;
  for (auto& [coordinator, state] : fetcher_state_map_) {
    if (!state.keyset || !state.keyset->HasKeys() ||
        state.expiration < base::Time::Now()) {
      FetchKeys(url::Origin(), coordinator, state, base::DoNothing());
    }
  }
}

void BiddingAndAuctionServerKeyFetcher::GetOrFetchKey(
    TrustedServerAPIType api,
    const url::Origin& scope_origin,
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
  if (!state.apis.Has(api)) {
    std::move(callback).Run(
        base::unexpected<std::string>("API not supported by coordinator"));
    return;
  }

  // If we have keys and they haven't expired just call the callback now.
  if (state.keyset && state.keyset->HasKeys() &&
      (state.debug_override || state.expiration > base::Time::Now())) {
    std::optional<BiddingAndAuctionServerKey> key =
        state.keyset->GetRandomKey(scope_origin);
    if (!key) {
      std::move(callback).Run(
          base::unexpected<std::string>("No key for adtech origin"));
      return;
    }

    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.
    base::UmaHistogramBoolean("Ads.InterestGroup.ServerAuction.KeyFetch.Cached",
                              true);
    std::move(callback).Run(std::move(*key));
    return;
  }

  if (!state.key_url.is_valid()) {
    std::move(callback).Run(
        base::unexpected<std::string>("Invalid Coordinator"));
    return;
  }

  base::UmaHistogramBoolean("Ads.InterestGroup.ServerAuction.KeyFetch.Cached",
                            false);
  FetchKeys(scope_origin, coordinator, state, std::move(callback));
}

void BiddingAndAuctionServerKeyFetcher::AddKeysDebugOverride(
    TrustedServerAPIType api,
    const url::Origin& coordinator,
    std::string serialized_keys,
    DebugOverrideCallback callback) {
  auto it = fetcher_state_map_.find(coordinator);
  if (it != fetcher_state_map_.end()) {
    std::move(callback).Run(
        "Can't add debug override because coordinator with origin "
        "already exists");
    return;
  }

  PerCoordinatorFetcherState state;
  state.version = 2;
  state.apis.Put(api);
  state.debug_override = true;
  state.debug_override_callback = std::move(callback);
  fetcher_state_map_.insert(std::pair(coordinator, std::move(state)));
  // Pretend we succeeded in loading from network.
  OnFetchKeysFromNetworkComplete(
      std::move(coordinator),
      std::make_optional<std::string>(std::move(serialized_keys)));
}

void BiddingAndAuctionServerKeyFetcher::FetchKeys(
    const url::Origin& scope_origin,
    const url::Origin& coordinator,
    PerCoordinatorFetcherState& state,
    BiddingAndAuctionServerKeyFetcherCallback callback) {
  state.fetch_start = base::TimeTicks::Now();
  state.queue.emplace_back(std::move(callback), scope_origin);
  // If we're already fetching or parsing a debug override, just wait for that
  // to finish.
  if (state.queue.size() > 1 || state.debug_override) {
    return;
  }

  state.keyset.reset();

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
    std::pair<base::Time, std::string> expiration_and_keys) {
  if (expiration_and_keys.first < base::Time::Now()) {
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ServerAuction.KeyFetch.DBCached", false);
    FetchKeysFromNetwork(coordinator);
    return;
  }

  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  std::optional<BiddingAndAuctionKeySet> keyset =
      BiddingAndAuctionKeySet::FromBinaryProto(expiration_and_keys.second);
  if (!keyset || keyset->SchemaVersion() != state.version) {
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ServerAuction.KeyFetch.DBCached", false);
    FetchKeysFromNetwork(coordinator);
    return;
  }

  base::UmaHistogramBoolean("Ads.InterestGroup.ServerAuction.KeyFetch.DBCached",
                            true);
  CacheKeysAndRunAllCallbacks(coordinator, std::move(*keyset),
                              expiration_and_keys.first);
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
    std::optional<std::string> response) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  bool was_cached = !state.debug_override && state.loader->LoadedFromCache();
  state.loader.reset();
  if (!response) {
    FailAllCallbacks(coordinator);
    return;
  }
  if (!state.debug_override) {
    base::UmaHistogramTimes(
        "Ads.InterestGroup.ServerAuction.KeyFetch.NetworkTime",
        base::TimeTicks::Now() - state.network_fetch_start);
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ServerAuction.KeyFetch.NetworkCached", was_cached);
  }
  if (state.version == 1) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response,
        base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnParsedKeys,
                       weak_ptr_factory_.GetWeakPtr(), coordinator));
  } else {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response,
        base::BindOnce(&BiddingAndAuctionServerKeyFetcher::OnParsedKeysV2,
                       weak_ptr_factory_.GetWeakPtr(), coordinator));
  }
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

  const base::Value::List* keys_list = response_dict->FindList("keys");
  if (!keys_list) {
    FailAllCallbacks(coordinator);
    return;
  }

  std::vector<BiddingAndAuctionServerKey> keys = ParseKeysList(keys_list);

  if (keys.size() == 0) {
    FailAllCallbacks(coordinator);
    return;
  }

  BiddingAndAuctionKeySet keyset(std::move(keys));
  base::Time expiration = base::Time::Now() + kKeyRequestInterval;
  if (base::FeatureList::IsEnabled(features::kFledgeStoreBandAKeysInDB) &&
      !fetcher_state_map_.at(coordinator).debug_override) {
    manager_->SetBiddingAndAuctionServerKeys(
        coordinator, keyset.AsBinaryProto(), expiration);
  }
  CacheKeysAndRunAllCallbacks(coordinator, std::move(keyset), expiration);
}

void BiddingAndAuctionServerKeyFetcher::OnParsedKeysV2(
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
  std::vector<std::pair<url::Origin, std::vector<BiddingAndAuctionServerKey>>>
      origin_scoped_keys;

  const base::Value::Dict* origin_scoped_keys_dict =
      response_dict->FindDict("originScopedKeys");
  if (!origin_scoped_keys_dict) {
    FailAllCallbacks(coordinator);
    return;
  }

  for (const auto [origin_str, per_origin_keys_value] :
       *origin_scoped_keys_dict) {
    url::Origin origin = url::Origin::Create(GURL(origin_str));

    if (origin.opaque()) {
      // Origins should never be opaque. That means parsing failed.
      FailAllCallbacks(coordinator);
      return;
    }

    if (!per_origin_keys_value.is_dict()) {
      FailAllCallbacks(coordinator);
      return;
    }
    const base::Value::List* keys_list =
        per_origin_keys_value.GetDict().FindList("keys");
    if (!keys_list) {
      FailAllCallbacks(coordinator);
      return;
    }

    std::vector<BiddingAndAuctionServerKey> keys = ParseKeysList(keys_list);

    if (keys.empty()) {
      FailAllCallbacks(coordinator);
      return;
    }

    origin_scoped_keys.emplace_back(origin, std::move(keys));
  }
  if (origin_scoped_keys.empty()) {
    FailAllCallbacks(coordinator);
    return;
  }

  BiddingAndAuctionKeySet keyset(std::move(origin_scoped_keys));
  base::Time expiration = base::Time::Now() + kKeyRequestInterval;
  if (base::FeatureList::IsEnabled(features::kFledgeStoreBandAKeysInDB) &&
      !fetcher_state_map_.at(coordinator).debug_override) {
    manager_->SetBiddingAndAuctionServerKeys(
        coordinator, keyset.AsBinaryProto(), expiration);
  }
  CacheKeysAndRunAllCallbacks(coordinator, std::move(keyset), expiration);
}

void BiddingAndAuctionServerKeyFetcher::CacheKeysAndRunAllCallbacks(
    const url::Origin& coordinator,
    BiddingAndAuctionKeySet keyset,
    base::Time expiration) {
  PerCoordinatorFetcherState& state = fetcher_state_map_.at(coordinator);
  DCHECK_EQ(keyset.SchemaVersion(), state.version);
  state.keyset = std::move(keyset);
  state.expiration = expiration;

  while (!state.queue.empty()) {
    // We call the callback *before* removing the current request from the list.
    // That avoids the problem if callback were to synchronously enqueue another
    // request. If we removed the current request first then enqueued the
    // request, that would start another thread of execution since there was an
    // empty queue.
    // Use a random key from the set to limit the server's ability to identify
    // us based on the key we use.

    std::optional<BiddingAndAuctionServerKey> key =
        state.keyset->GetRandomKey(state.queue.front().scope_origin);
    if (!key) {
      std::move(state.queue.front().callback)
          .Run(base::unexpected<std::string>("No key for adtech origin"));
    } else {
      std::move(state.queue.front().callback).Run(std::move(*key));
    }
    state.queue.pop_front();
  }

  if (state.debug_override_callback) {
    std::move(state.debug_override_callback).Run(std::nullopt);
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
    std::move(state.queue.front().callback)
        .Run(base::unexpected<std::string>("Key fetch failed"));
    state.queue.pop_front();
  }

  if (state.debug_override_callback) {
    std::move(state.debug_override_callback).Run("Key config decoding failed");
  }
}

}  // namespace content
