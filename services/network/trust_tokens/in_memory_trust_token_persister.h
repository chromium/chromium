// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_persister.h"

namespace network {

// An InMemoryTrustTokenPersister stores Trust Tokens state during its lifetime,
// but does not write it through to a backend. It is suitable for use in tests
// (as a fake) and in environments without access to SQL.
class InMemoryTrustTokenPersister : public TrustTokenPersister {
 public:
  InMemoryTrustTokenPersister();
  ~InMemoryTrustTokenPersister() override;

  // TrustTokenPersister implementation:
  std::unique_ptr<TrustTokenIssuerConfig> GetIssuerConfig(
      const SuitableTrustTokenOrigin& issuer) override;
  std::unique_ptr<TrustTokenToplevelConfig> GetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel) override;
  std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
  GetIssuerToplevelPairConfig(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& toplevel) override;

  void SetIssuerConfig(const SuitableTrustTokenOrigin& issuer,
                       std::unique_ptr<TrustTokenIssuerConfig> config) override;
  void SetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenToplevelConfig> config) override;
  void SetIssuerToplevelPairConfig(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) override;

  bool DeleteIssuerConfig(PSTKeyMatcher key_matcher,
                          PSTTimeMatcher time_matcher) override;
  bool DeleteToplevelConfig(PSTKeyMatcher key_matcher) override;
  bool DeleteIssuerToplevelPairConfig(PSTKeyMatcher key_matcher,
                                      PSTTimeMatcher time_matcher) override;

  base::flat_map<SuitableTrustTokenOrigin, int> GetStoredTrustTokenCounts()
      override;

  IssuerRedemptionRecordMap GetRedemptionRecords() override;

 private:
  std::map<SuitableTrustTokenOrigin, std::unique_ptr<TrustTokenToplevelConfig>>
      toplevel_configs_;
  std::map<SuitableTrustTokenOrigin, std::unique_ptr<TrustTokenIssuerConfig>>
      issuer_configs_;
  std::map<std::pair<SuitableTrustTokenOrigin, SuitableTrustTokenOrigin>,
           std::unique_ptr<TrustTokenIssuerToplevelPairConfig>>
      issuer_toplevel_pair_configs_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_
