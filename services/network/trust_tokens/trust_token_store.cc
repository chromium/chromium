// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_store.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/types.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "url/origin.h"

namespace network {

namespace {
class NeverExpiringExpiryDelegate
    : public TrustTokenStore::RecordExpiryDelegate {
 public:
  bool IsRecordExpired(const TrustTokenRedemptionRecord& record,
                       const base::TimeDelta& time_since_last_redemption,
                       const SuitableTrustTokenOrigin& issuer) override {
    return false;
  }
};

}  // namespace

TrustTokenStore::TrustTokenStore(
    std::unique_ptr<TrustTokenPersister> persister,
    std::unique_ptr<RecordExpiryDelegate> expiry_delegate)
    : persister_(std::move(persister)),
      record_expiry_delegate_(std::move(expiry_delegate)) {
  DCHECK(persister_);
}

TrustTokenStore::~TrustTokenStore() = default;

std::unique_ptr<TrustTokenStore> TrustTokenStore::CreateForTesting(
    std::unique_ptr<TrustTokenPersister> persister,
    std::unique_ptr<RecordExpiryDelegate> expiry_delegate) {
  if (!persister)
    persister = std::make_unique<InMemoryTrustTokenPersister>();
  if (!expiry_delegate)
    expiry_delegate = std::make_unique<NeverExpiringExpiryDelegate>();
  return std::make_unique<TrustTokenStore>(std::move(persister),
                                           std::move(expiry_delegate));
}

void TrustTokenStore::RecordIssuance(const SuitableTrustTokenOrigin& issuer) {
  SuitableTrustTokenOrigin issuer_origin = issuer;
  std::unique_ptr<TrustTokenIssuerConfig> config =
      persister_->GetIssuerConfig(issuer);
  if (!config)
    config = std::make_unique<TrustTokenIssuerConfig>();
  *config->mutable_last_issuance() =
      internal::TimeToTimestamp(base::Time::Now());
  persister_->SetIssuerConfig(issuer, std::move(config));
}

std::optional<base::TimeDelta> TrustTokenStore::TimeSinceLastIssuance(
    const SuitableTrustTokenOrigin& issuer) {
  std::unique_ptr<TrustTokenIssuerConfig> config =
      persister_->GetIssuerConfig(issuer);
  if (!config)
    return std::nullopt;
  if (!config->has_last_issuance())
    return std::nullopt;

  base::Time last_issuance = internal::TimestampToTime(config->last_issuance());
  base::TimeDelta ret = base::Time::Now() - last_issuance;
  if (ret.is_negative())
    return std::nullopt;

  return ret;
}

bool TrustTokenStore::IsRedemptionLimitHit(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) const {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    return false;
  if (!config->has_last_redemption())
    return false;
  if (!config->has_penultimate_redemption())
    return false;

  base::Time penultimate_redemption =
      internal::TimestampToTime(config->penultimate_redemption());
  base::TimeDelta ret = base::Time::Now() - penultimate_redemption;
  if (ret.is_negative())
    return false;
  if (ret > base::Seconds(
                kTrustTokenPerIssuerToplevelRedemptionFrequencyLimitInSeconds))
    return false;
  return true;
}

std::optional<base::TimeDelta> TrustTokenStore::TimeSinceLastRedemption(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    return std::nullopt;
  if (!config->has_last_redemption())
    return std::nullopt;

  base::Time last_redemption =
      internal::TimestampToTime(config->last_redemption());
  base::TimeDelta ret = base::Time::Now() - last_redemption;
  if (ret.is_negative())
    return std::nullopt;
  return ret;
}

bool TrustTokenStore::IsAssociated(const SuitableTrustTokenOrigin& issuer,
                                   const SuitableTrustTokenOrigin& top_level) {
  std::unique_ptr<TrustTokenToplevelConfig> config =
      persister_->GetToplevelConfig(top_level);
  if (!config)
    return false;
  return base::Contains(config->associated_issuers(), issuer.Serialize());
}

bool TrustTokenStore::SetAssociation(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  std::unique_ptr<TrustTokenToplevelConfig> config =
      persister_->GetToplevelConfig(top_level);
  if (!config)
    config = std::make_unique<TrustTokenToplevelConfig>();
  auto string_issuer = issuer.Serialize();

  if (base::Contains(config->associated_issuers(), string_issuer))
    return true;

  if (config->associated_issuers_size() >=
      kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers) {
    return false;
  }

  config->add_associated_issuers(std::move(string_issuer));
  persister_->SetToplevelConfig(top_level, std::move(config));

  return true;
}

void TrustTokenStore::PruneStaleIssuerState(
    const SuitableTrustTokenOrigin& issuer,
    const std::vector<mojom::TrustTokenVerificationKeyPtr>& keys) {
  DCHECK([&keys]() {
    std::set<std::string_view> unique_keys;
    for (const auto& key : keys)
      unique_keys.insert(std::string_view(key->body));
    return unique_keys.size() == keys.size();
  }());

  std::unique_ptr<TrustTokenIssuerConfig> config =
      persister_->GetIssuerConfig(issuer);
  if (!config)
    config = std::make_unique<TrustTokenIssuerConfig>();

  google::protobuf::RepeatedPtrField<TrustToken> filtered_tokens;
  for (auto& token : *config->mutable_tokens()) {
    if (base::Contains(keys, token.signing_key(),
                       &mojom::TrustTokenVerificationKey::body)) {
      *filtered_tokens.Add() = std::move(token);
    }
  }

  config->mutable_tokens()->Swap(&filtered_tokens);

  persister_->SetIssuerConfig(issuer, std::move(config));
}

void TrustTokenStore::AddTokens(const SuitableTrustTokenOrigin& issuer,
                                base::span<const std::string> token_bodies,
                                std::string_view issuing_key) {
  auto config = persister_->GetIssuerConfig(issuer);
  if (!config)
    config = std::make_unique<TrustTokenIssuerConfig>();

  for (auto it = token_bodies.begin();
       it != token_bodies.end() &&
       config->tokens_size() < kTrustTokenPerIssuerTokenCapacity;
       ++it) {
    TrustToken* entry = config->add_tokens();
    entry->set_body(*it);
    entry->set_signing_key(std::string(issuing_key));
    *entry->mutable_creation_time() =
        internal::TimeToTimestamp(base::Time::Now());
  }

  persister_->SetIssuerConfig(issuer, std::move(config));
}

int TrustTokenStore::CountTokens(const SuitableTrustTokenOrigin& issuer) {
  auto config = persister_->GetIssuerConfig(issuer);
  if (!config)
    return 0;
  return config->tokens_size();
}

std::vector<TrustToken> TrustTokenStore::RetrieveMatchingTokens(
    const SuitableTrustTokenOrigin& issuer,
    base::RepeatingCallback<bool(const std::string&)> key_matcher) {
  auto config = persister_->GetIssuerConfig(issuer);
  std::vector<TrustToken> matching_tokens;
  if (!config)
    return matching_tokens;

  base::ranges::copy_if(config->tokens(), std::back_inserter(matching_tokens),
                        [&key_matcher](const TrustToken& token) {
                          return token.has_signing_key() &&
                                 key_matcher.Run(token.signing_key());
                        });

  return matching_tokens;
}

void TrustTokenStore::DeleteToken(const SuitableTrustTokenOrigin& issuer,
                                  const TrustToken& to_delete) {
  auto config = persister_->GetIssuerConfig(issuer);
  if (!config)
    return;

  for (auto it = config->mutable_tokens()->begin();
       it != config->mutable_tokens()->end(); ++it) {
    if (it->body() == to_delete.body()) {
      config->mutable_tokens()->erase(it);
      break;
    }
  }

  persister_->SetIssuerConfig(issuer, std::move(config));
}

void TrustTokenStore::SetRedemptionRecord(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level,
    const TrustTokenRedemptionRecord& record) {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config) {
    config = std::make_unique<TrustTokenIssuerToplevelPairConfig>();
    *config->mutable_last_redemption() =
        internal::TimeToTimestamp(base::Time::UnixEpoch());
  }
  *config->mutable_redemption_record() = record;
  *config->mutable_penultimate_redemption() = config->last_redemption();
  *config->mutable_last_redemption() =
      internal::TimeToTimestamp(base::Time::Now());
  persister_->SetIssuerToplevelPairConfig(issuer, top_level, std::move(config));
}

std::optional<TrustTokenRedemptionRecord>
TrustTokenStore::RetrieveNonstaleRedemptionRecord(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    return std::nullopt;

  if (!config->has_redemption_record())
    return std::nullopt;

  std::optional<base::TimeDelta> maybe_time_since_last_redemption =
      TimeSinceLastRedemption(issuer, top_level);
  base::TimeDelta time_since_last_redemption = base::Seconds(0);
  if (maybe_time_since_last_redemption)
    time_since_last_redemption = *maybe_time_since_last_redemption;

  if (record_expiry_delegate_->IsRecordExpired(
          config->redemption_record(), time_since_last_redemption, issuer))
    return std::nullopt;

  return config->redemption_record();
}

bool TrustTokenStore::ClearDataForFilter(mojom::ClearDataFilterPtr filter) {
  const base::Time windows_epoch =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(0));
  const base::Time beginning_of_time = windows_epoch;
  const base::Time end_of_time = base::Time::Now();
  if (!filter) {
    const auto key_matcher = base::BindRepeating(
        [](const SuitableTrustTokenOrigin&) { return true; });
    const auto time_matcher =
        base::BindRepeating([](const base::Time&) { return true; });
    return persister_->DeleteForOrigins(std::move(key_matcher),
                                        std::move(time_matcher));
  }
  // Returns whether |storage_key|'s data should be deleted, based on the logic
  // |filter| specifies. (Default to deleting everything, because a null
  // |filter| is a wildcard.)
  auto key_matcher = base::BindRepeating(
      [](const mojom::ClearDataFilter& filter,
         const SuitableTrustTokenOrigin& storage_key) -> bool {
        // Match an origin if
        // - it is an eTLD+1 (aka "domain and registry") match with anything
        // on |filter|'s domain list, or
        // - it is an origin match with anything on |filter|'s origin list.
        bool is_match = base::Contains(filter.origins, storage_key.origin());

        // Computing the domain might be a little expensive, so
        // skip it if we know for sure the origin is a match because it
        // matches the origin list.
        if (!is_match) {
          std::string etld1_for_origin =
              net::registry_controlled_domains::GetDomainAndRegistry(
                  storage_key.origin(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
          is_match = base::Contains(filter.domains, etld1_for_origin);
        }

        switch (filter.type) {
          case mojom::ClearDataFilter::Type::KEEP_MATCHES:
            return !is_match;
          case mojom::ClearDataFilter::Type::DELETE_MATCHES:
            return is_match;
        }
      },
      *filter);

  auto time_matcher = base::BindRepeating(
      [](const base::Time& begin_time, const base::Time& end_time,
         const base::Time& creation_time) -> bool {
        const base::TimeDelta creation_delta =
            creation_time.ToDeltaSinceWindowsEpoch();
        const base::TimeDelta begin_delta =
            begin_time.ToDeltaSinceWindowsEpoch();
        const base::TimeDelta end_delta = end_time.ToDeltaSinceWindowsEpoch();
        if ((creation_delta < begin_delta) || (creation_delta > end_delta)) {
          return false;
        }
        return true;
      },
      beginning_of_time, end_of_time);
  return persister_->DeleteForOrigins(std::move(key_matcher),
                                      std::move(time_matcher));
}

// Assumes predicate is created from
// CookieSettings::CreateDeleteCookieOnExitPredicate and matches PST
// storage key hosts.
//
// Some inputs and the resulting data clearing decisions.
//
// Serialized storage key    | Clear on exit list includes | will get cleared
// https://a.com:1443        | a.com                       | yes
// https://a.com:1443        | https://a.com               | yes
// https://a.b.com:1443      | a.b.com                     | yes
// https://a.b.com:1443      | b.com                       | no
// https://b.com:1443        | a.b.com                     | no
//
bool TrustTokenStore::ClearDataForPredicate(
    base::RepeatingCallback<bool(const std::string&)> predicate) {
  auto time_matcher = base::BindRepeating(
      [](const base::Time& creation_time) -> bool { return true; });
  auto key_matcher = base::BindRepeating(
      [](base::RepeatingCallback<bool(const std::string&)> pred,
         const SuitableTrustTokenOrigin& storage_key) -> bool {
        return pred.Run(storage_key.origin().host());
      },
      predicate);
  return persister_->DeleteForOrigins(std::move(key_matcher),
                                      std::move(time_matcher));
}

bool TrustTokenStore::DeleteStoredTrustTokens(
    const SuitableTrustTokenOrigin& issuer) {
  auto issuer_config = persister_->GetIssuerConfig(issuer);
  if (!issuer_config)
    return false;

  const bool had_stored_tokens = issuer_config->tokens_size() > 0;
  issuer_config->mutable_tokens()->Clear();
  persister_->SetIssuerConfig(issuer, std::move(issuer_config));
  return had_stored_tokens;
}

base::flat_map<SuitableTrustTokenOrigin, int>
TrustTokenStore::GetStoredTrustTokenCounts() {
  return persister_->GetStoredTrustTokenCounts();
}

IssuerRedemptionRecordMap TrustTokenStore::GetRedemptionRecords() {
  return persister_->GetRedemptionRecords();
}

}  // namespace network
