// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"

namespace network {

// Class TrustTokenKeyCommitments is a singleton owned by NetworkService; it
// stores all known information about issuers' Trust Tokens key state. This
// state is provided through offline updates via |Set|.
class TrustTokenKeyCommitments
    : public TrustTokenKeyCommitmentGetter,
      public SynchronousTrustTokenKeyCommitmentGetter {
 public:
  TrustTokenKeyCommitments();
  ~TrustTokenKeyCommitments() override;

  TrustTokenKeyCommitments(const TrustTokenKeyCommitments&) = delete;
  TrustTokenKeyCommitments& operator=(const TrustTokenKeyCommitments&) = delete;

  // Overwrites the current issuers-to-commitments map with the values in |map|,
  // ignoring those issuer origins which are not suitable Trust Tokens origins
  // (in the sense of SuitableTrustTokenOrigin).
  void Set(
      base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> map);

  // Overwrites the current issuers-to-commitments map with the values in
  // |raw_commitments|, which should be the JSON-encoded string representation
  // of a collection of issuers' key commitments according to the format
  // specified, for now, in the Trust Tokens design doc:
  // https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.z52drgpfgulz.
  void ParseAndSet(std::string_view raw_commitments);

  // TrustTokenKeyCommitmentGetter implementation:
  //
  // If |origin| is a suitable Trust Tokens origin (in the sense of
  // SuitableTrustTokenOrigin), searches for a key commitment result
  // corresponding to |origin|.
  //
  // If |origin| is not suitable, or if no commitment result is found, returns
  // nullptr. Otherwise, returns the key commitment result stored for |origin|,
  // with its verification keys filtered to contain at most the maximum number
  // of keys allowed for the protocol version, none of which has yet expired.
  //
  // If commitments for |origin| were passed both through a prior call to |Set|
  // and through the --additional-private-state-token-key-commitments
  // command-line switch, the commitments passed through the switch take
  // precedence.
  //
  // Implementation note: this is a thin wrapper around GetSync.
  void Get(const url::Origin& origin,
           base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
               done) const override;

  // SynchronousTrustTokenKeyCommitmentResultGetter implementation:
  //
  // Implementation note: This is where the guts of |Get| live.
  mojom::TrustTokenKeyCommitmentResultPtr GetSync(
      const url::Origin& origin) const override;

 private:
  base::flat_map<SuitableTrustTokenOrigin,
                 mojom::TrustTokenKeyCommitmentResultPtr>
      commitments_;

  // Additional commitments provided (for manual experimentation or testing)
  // through the command-line switch.
  const base::flat_map<SuitableTrustTokenOrigin,
                       mojom::TrustTokenKeyCommitmentResultPtr>
      additional_commitments_from_command_line_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_
