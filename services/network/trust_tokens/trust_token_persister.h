// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network {

class TrustTokenIssuerConfig;
class TrustTokenToplevelConfig;
class TrustTokenIssuerToplevelPairConfig;

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

  // Deletes any data stored keyed by matching origins (as issuers or top-level
  // origins).
  virtual bool DeleteForOrigins(
      base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)>
          matcher) = 0;

  virtual base::flat_map<SuitableTrustTokenOrigin, int>
  GetStoredTrustTokenCounts() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PERSISTER_H_
