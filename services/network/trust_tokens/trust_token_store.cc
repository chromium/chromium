// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_store.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
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
  config->set_last_issuance(internal::TimeToString(base::Time::Now()));
  persister_->SetIssuerConfig(issuer, std::move(config));
}

base::Optional<base::TimeDelta> TrustTokenStore::TimeSinceLastIssuance(
    const SuitableTrustTokenOrigin& issuer) {
  std::unique_ptr<TrustTokenIssuerConfig> config =
      persister_->GetIssuerConfig(issuer);
  if (!config)
    return base::nullopt;
  if (!config->has_last_issuance())
    return base::nullopt;
  base::Optional<base::Time> maybe_last_issuance =
      internal::StringToTime(config->last_issuance());
  if (!maybe_last_issuance)
    return base::nullopt;

  base::TimeDelta ret = base::Time::Now() - *maybe_last_issuance;
  if (ret < base::TimeDelta())
    return base::nullopt;

  return ret;
}

void TrustTokenStore::RecordRedemption(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config =
      persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    config = std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  config->set_last_redemption(internal::TimeToString(base::Time::Now()));
  persister_->SetIssuerToplevelPairConfig(issuer, top_level, std::move(config));
}

base::Optional<base::TimeDelta> TrustTokenStore::TimeSinceLastRedemption(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    return base::nullopt;
  if (!config->has_last_redemption())
    return base::nullopt;
  base::Optional<base::Time> maybe_last_redemption =
      internal::StringToTime(config->last_redemption());
  // internal::StringToTime can fail in the case of data corruption (or writer
  // error).
  if (!maybe_last_redemption)
    return base::nullopt;

  base::TimeDelta ret = base::Time::Now() - *maybe_last_redemption;
  if (ret < base::TimeDelta())
    return base::nullopt;
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
    std::set<base::StringPiece> unique_keys;
    for (const auto& key : keys)
      unique_keys.insert(base::StringPiece(key->body));
    return unique_keys.size() == keys.size();
  }());

  std::unique_ptr<TrustTokenIssuerConfig> config =
      persister_->GetIssuerConfig(issuer);
  if (!config)
    config = std::make_unique<TrustTokenIssuerConfig>();

  google::protobuf::RepeatedPtrField<TrustToken> filtered_tokens;
  for (auto& token : *config->mutable_tokens()) {
    if (std::any_of(keys.begin(), keys.end(),
                    [&token](const mojom::TrustTokenVerificationKeyPtr& key) {
                      return key->body == token.signing_key();
                    }))
      *filtered_tokens.Add() = std::move(token);
  }

  config->mutable_tokens()->Swap(&filtered_tokens);

  persister_->SetIssuerConfig(issuer, std::move(config));
}

void TrustTokenStore::AddTokens(const SuitableTrustTokenOrigin& issuer,
                                base::span<const std::string> token_bodies,
                                base::StringPiece issuing_key) {
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

  std::copy_if(config->tokens().begin(), config->tokens().end(),
               std::back_inserter(matching_tokens),
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
  if (!config)
    config = std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  *config->mutable_redemption_record() = record;
  persister_->SetIssuerToplevelPairConfig(issuer, top_level, std::move(config));
}

base::Optional<TrustTokenRedemptionRecord>
TrustTokenStore::RetrieveNonstaleRedemptionRecord(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& top_level) {
  auto config = persister_->GetIssuerToplevelPairConfig(issuer, top_level);
  if (!config)
    return base::nullopt;

  if (!config->has_redemption_record())
    return base::nullopt;

  if (record_expiry_delegate_->IsRecordExpired(config->redemption_record(),
                                               issuer))
    return base::nullopt;

  return config->redemption_record();
}

bool TrustTokenStore::ClearDataForFilter(mojom::ClearDataFilterPtr filter) {
  if (!filter) {
    return persister_->DeleteForOrigins(base::BindRepeating(
        [](const SuitableTrustTokenOrigin&) { return true; }));
  }

  // Returns whether |storage_key|'s data should be deleted, based on the logic
  // |filter| specifies. (Default to deleting everything, because a null
  // |filter| is a wildcard.)
  auto matcher = base::BindRepeating(
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

  return persister_->DeleteForOrigins(std::move(matcher));
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

}  // namespace network
