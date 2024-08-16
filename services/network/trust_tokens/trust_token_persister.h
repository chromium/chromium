// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network {

class TrustTokenIssuerConfig;
class TrustTokenToplevelConfig;
class TrustTokenIssuerToplevelPairConfig;
typedef base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)>
    PSTKeyMatcher;
typedef base::RepeatingCallback<bool(const base::Time&)> PSTTimeMatcher;

// Used by PST Dev UI, stores toplevel origin and last redemption per issuer.
typedef base::flat_map<url::Origin,
                       std::vector<mojom::ToplevelRedemptionRecordPtr>>
    IssuerRedemptionRecordMap;

// Interface TrustTokenPersister defines interaction with a backing store for
// Trust Tokens state. The most-frequently-used implementation will
// be on top of SQLite; there is also an ephemeral implementation for
// tests and environments not built with SQLite.
class TrustTokenPersister {
 public:
  TrustTokenPersister() = default;
  virtual ~TrustTokenPersister() = default;

  TrustTokenPersister(const TrustTokenPersister&) = delete;
  TrustTokenPersister& operator=(const TrustTokenPersister&) = delete;

  virtual std::unique_ptr<TrustTokenIssuerConfig> GetIssuerConfig(
      const SuitableTrustTokenOrigin& issuer) = 0;
  virtual std::unique_ptr<TrustTokenToplevelConfig> GetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel) = 0;
  virtual std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
  GetIssuerToplevelPairConfig(const SuitableTrustTokenOrigin& issuer,
                              const SuitableTrustTokenOrigin& toplevel) = 0;

  virtual void SetIssuerConfig(
      const SuitableTrustTokenOrigin& issuer,
      std::unique_ptr<TrustTokenIssuerConfig> config) = 0;
  virtual void SetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenToplevelConfig> config) = 0;
  virtual void SetIssuerToplevelPairConfig(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) = 0;

  // Implementation of this should either update or delete the issuer config
  // depending on the matchers. Key matcher is used to find the right issuer
  // config, and time matcher is used when deciding to delete the stored tokens.
  // Tokens are deleted if time matcher returns true. The config is deleted if
  // all tokens are deleted or there were no tokens to begin with.
  //
  // If a token (from a key matched issuer config) does not have a creation
  // time, it is deleted.
  virtual bool DeleteIssuerConfig(PSTKeyMatcher key_matcher,
                                  PSTTimeMatcher time_matcher) = 0;
  virtual bool DeleteToplevelConfig(PSTKeyMatcher key_matcher) = 0;

  // Implementation of this should delete the issuer config depending on the
  // matchers. Key matcher is used to find the right issuer toplevel pair
  // config, and time matcher is used to check the creation time of the stored
  // redemption record. The config is deleted if time matcher returns true.
  //
  // The config is deleted if it does not have a redemption record. The config
  // is deleted if the stored redemption record does not have creation time.
  //
  virtual bool DeleteIssuerToplevelPairConfig(PSTKeyMatcher key_matcher,
                                              PSTTimeMatcher time_matcher) = 0;

  virtual base::flat_map<SuitableTrustTokenOrigin, int>
  GetStoredTrustTokenCounts() = 0;

  // Used by PST Dev UI to surface toplevel config information
  virtual IssuerRedemptionRecordMap GetRedemptionRecords() = 0;

  bool DeleteForOrigins(PSTKeyMatcher key_matcher,
                        PSTTimeMatcher time_matcher) {
    bool any_data_was_deleted = false;
    any_data_was_deleted |= DeleteIssuerConfig(key_matcher, time_matcher);
    any_data_was_deleted |= DeleteToplevelConfig(key_matcher);
    any_data_was_deleted |=
        DeleteIssuerToplevelPairConfig(key_matcher, time_matcher);
    return any_data_was_deleted;
  }
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_
