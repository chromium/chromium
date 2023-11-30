// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_

#include <memory>
#include <string_view>

#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"

namespace network {

// These field names are from the key commitment JSON format specified in the
// Trust Tokens design doc
// (https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.6wh9crbxdizi).
// "protocol version" (version of Trust Token used for this commitment):
extern const char kTrustTokenKeyCommitmentProtocolVersionField[];
// This commitment's ID, used for mediating between concurrencyID for this key
// commitment):
extern const char kTrustTokenKeyCommitmentIDField[];
// "Batch size" (number of blinded tokens to provide per issuance request):
extern const char kTrustTokenKeyCommitmentBatchsizeField[];
// "keys" (dictionary of keys)
extern const char kTrustTokenKeyCommitmentKeysField[];
// Each issuance key's expiry timestamp:
extern const char kTrustTokenKeyCommitmentExpiryField[];
// Each issuance key's key material:
extern const char kTrustTokenKeyCommitmentKeyField[];

// WARNING WARNING WARNING: When updating the parser implementation, please make
// sure the normative source(s) of the key commitment result data structure's
// format (as of writing, the design doc and perhaps ISSUER_PROTOCOL.md in the
// WICG repository) have been updated to reflect the change.
class TrustTokenKeyCommitmentParser
    : public TrustTokenKeyCommitmentController::Parser {
 public:
  TrustTokenKeyCommitmentParser() = default;
  ~TrustTokenKeyCommitmentParser() override = default;

  // Parses a JSON key commitment response, returning nullptr if the input is
  // not a valid representation of a JSON dictionary containing all required
  // fields listed in the Trust Tokens design doc, the current normative source
  // for key commitment responses' format:
  //
  // https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.wkezf6pcskvh
  mojom::TrustTokenKeyCommitmentResultPtr Parse(
      std::string_view response_body) override;

  // Like |Parse|, except that the input is expected to be of the form
  // { "https://some-issuer.example": <JSON in the form expected by |Parse|>
  //   "https://some-other-issuer.example":
  //     <JSON in the form expected by |Parse|>,
  //   ...  }
  //
  // Returns nullptr if the input is not a dictionary.
  //
  // WARNING: If there are multiple keys that are exactly equal strings,
  // deduplicates these entries arbitrarily (due to the behavior of
  // base::JSONReader). For instance, if these keys are arriving through the
  // component updater, you might want to guarantee that the server-side logic
  // producing these structures guarantees no duplicate keys.
  //
  // If there are multiple keys that are not exact duplicates but correspond to
  // the same issuer, drops all but the entry with the largest key
  // lexicographically.
  //
  // Skips key-value pairs where the key is not a suitable Trust Tokens origin
  // or the value fails to parse.
  std::unique_ptr<base::flat_map<SuitableTrustTokenOrigin,
                                 mojom::TrustTokenKeyCommitmentResultPtr>>
  ParseMultipleIssuers(std::string_view response_body);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_
