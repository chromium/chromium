// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "base/containers/cxx20_erase.h"
#include "services/network/trust_tokens/types.h"

#include "base/containers/cxx20_erase_map.h"
#include "url/gurl.h"

namespace network {

InMemoryTrustTokenPersister::InMemoryTrustTokenPersister() = default;
InMemoryTrustTokenPersister::~InMemoryTrustTokenPersister() = default;

std::unique_ptr<TrustTokenToplevelConfig>
InMemoryTrustTokenPersister::GetToplevelConfig(
    const SuitableTrustTokenOrigin& toplevel) {
  auto it = toplevel_configs_.find(toplevel);
  if (it == toplevel_configs_.end())
    return nullptr;
  return std::make_unique<TrustTokenToplevelConfig>(*it->second);
}

std::unique_ptr<TrustTokenIssuerConfig>
InMemoryTrustTokenPersister::GetIssuerConfig(
    const SuitableTrustTokenOrigin& issuer) {
  auto it = issuer_configs_.find(issuer);
  if (it == issuer_configs_.end())
    return nullptr;
  return std::make_unique<TrustTokenIssuerConfig>(*it->second);
}

std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
InMemoryTrustTokenPersister::GetIssuerToplevelPairConfig(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& toplevel) {
  auto it =
      issuer_toplevel_pair_configs_.find(std::make_pair(issuer, toplevel));
  if (it == issuer_toplevel_pair_configs_.end())
    return nullptr;
  return std::make_unique<TrustTokenIssuerToplevelPairConfig>(*it->second);
}

void InMemoryTrustTokenPersister::SetToplevelConfig(
    const SuitableTrustTokenOrigin& toplevel,
    std::unique_ptr<TrustTokenToplevelConfig> config) {
  toplevel_configs_[toplevel] = std::move(config);
}

void InMemoryTrustTokenPersister::SetIssuerConfig(
    const SuitableTrustTokenOrigin& issuer,
    std::unique_ptr<TrustTokenIssuerConfig> config) {
  issuer_configs_[issuer] = std::move(config);
}

void InMemoryTrustTokenPersister::SetIssuerToplevelPairConfig(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& toplevel,
    std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) {
  issuer_toplevel_pair_configs_[std::make_pair(issuer, toplevel)] =
      std::move(config);
}

bool InMemoryTrustTokenPersister::DeleteIssuerConfig(
    PSTKeyMatcher key_matcher,
    PSTTimeMatcher time_matcher) {
  std::vector<SuitableTrustTokenOrigin> keys_to_delete;
  std::vector<std::pair<SuitableTrustTokenOrigin, TrustTokenIssuerConfig>>
      key_value_pairs_to_update;
  bool data_deleted = false;

  for (const auto& kv : issuer_configs_) {
    if (!key_matcher.Run(kv.first)) {
      continue;
    }

    TrustTokenIssuerConfig new_issuer_config(*kv.second);
    // clear all tokens first, we will add them back if they are not a match
    new_issuer_config.clear_tokens();
    for (const auto& token : kv.second->tokens()) {
      if (token.has_creation_time()) {
        const base::Time creation_time =
            internal::TimestampToTime(token.creation_time());
        if (!time_matcher.Run(creation_time)) {
          // add token back to new issuer config
          TrustToken* new_token = new_issuer_config.add_tokens();
          *new_token = token;
        }  // else token is deleted, do not add token back to new_issuer_config
      }    // else token has no creation time, delete it
    }
    if (new_issuer_config.tokens().size() == 0) {
      // all tokens are deleted, or there were no tokens to begin with
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    } else if (new_issuer_config.tokens().size() !=
               kv.second->tokens().size()) {
      // some tokens deleted, at least one token remains
      key_value_pairs_to_update.emplace_back(kv.first,
                                             std::move(new_issuer_config));
      data_deleted = true;
    }
    // else no change in config
  }

  for (auto const& origin : keys_to_delete) {
    auto it = issuer_configs_.find(origin);
    DCHECK(it != issuer_configs_.end());
    issuer_configs_.erase(it);
  }
  for (const auto& kv : key_value_pairs_to_update) {
    issuer_configs_[kv.first] =
        std::make_unique<TrustTokenIssuerConfig>(std::move(kv.second));
  }

  return data_deleted;
}

bool InMemoryTrustTokenPersister::DeleteToplevelConfig(
    PSTKeyMatcher key_matcher) {
  std::vector<SuitableTrustTokenOrigin> keys_to_delete;
  bool data_deleted = false;

  for (const auto& kv : toplevel_configs_) {
    if (key_matcher.Run(kv.first)) {
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    }
  }
  for (auto const& origin : keys_to_delete) {
    auto it = toplevel_configs_.find(origin);
    DCHECK(it != toplevel_configs_.end());
    toplevel_configs_.erase(it);
  }
  return data_deleted;
}

bool InMemoryTrustTokenPersister::DeleteIssuerToplevelPairConfig(
    PSTKeyMatcher key_matcher,
    PSTTimeMatcher time_matcher) {
  bool data_deleted = false;
  std::vector<std::pair<SuitableTrustTokenOrigin, SuitableTrustTokenOrigin>>
      keys_to_delete;
  for (const auto& kv : issuer_toplevel_pair_configs_) {
    const TrustTokenIssuerToplevelPairConfig* pair_config = kv.second.get();

    if (!key_matcher.Run(kv.first.first) && !key_matcher.Run(kv.first.second)) {
      continue;
    }

    if (!pair_config->has_redemption_record()) {
      // config does not have redemption record, delete it
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
      continue;
    }

    auto redemption_record = pair_config->redemption_record();
    if (redemption_record.has_creation_time()) {
      const base::Time creation_time =
          internal::TimestampToTime(redemption_record.creation_time());

      if (time_matcher.Run(creation_time)) {
        keys_to_delete.push_back(kv.first);
        data_deleted = true;
      }
    } else {
      // no creation time for RR, delete it
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    }
  }
  for (auto const& key : keys_to_delete) {
    auto it = issuer_toplevel_pair_configs_.find(key);
    DCHECK(it != issuer_toplevel_pair_configs_.end());
    issuer_toplevel_pair_configs_.erase(it);
  }
  return data_deleted;
}

base::flat_map<SuitableTrustTokenOrigin, int>
InMemoryTrustTokenPersister::GetStoredTrustTokenCounts() {
  base::flat_map<SuitableTrustTokenOrigin, int> result;
  for (const auto& kv : issuer_configs_) {
    result.emplace(kv.first, kv.second->tokens_size());
  }
  return result;
}

}  // namespace network
