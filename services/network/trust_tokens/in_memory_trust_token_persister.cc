// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/in_memory_trust_token_persister.h"

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

bool InMemoryTrustTokenPersister::DeleteForOrigins(
    base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)> matcher) {
  bool deleted_any_data = false;

  auto predicate_for_origin_keyed_maps = [&matcher](const auto& entry) {
    return matcher.Run(entry.first);
  };
  deleted_any_data |=
      base::EraseIf(issuer_configs_, predicate_for_origin_keyed_maps);
  deleted_any_data |=
      base::EraseIf(toplevel_configs_, predicate_for_origin_keyed_maps);

  deleted_any_data |= base::EraseIf(
      issuer_toplevel_pair_configs_, [&matcher](const auto& entry) {
        const std::pair<SuitableTrustTokenOrigin, SuitableTrustTokenOrigin>&
            key = entry.first;
        return matcher.Run(key.first) || matcher.Run(key.second);
      });

  return deleted_any_data;
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
