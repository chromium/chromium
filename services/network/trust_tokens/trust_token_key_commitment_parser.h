// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_

#include <memory>

#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"

namespace network {

// Field names from the key commitment JSON format specified in the Trust Tokens
// design doc
// (https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.6wh9crbxdizi):
// - "protocol_version" (version of Trust Token used for this commitment)
extern const char kTrustTokenKeyCommitmentProtocolVersionField[];
// - "id" (ID for this key commitment)
extern const char kTrustTokenKeyCommitmentIDField[];
// - "batch size" (number of blinded tokens to provide per issuance request)
extern const char kTrustTokenKeyCommitmentBatchsizeField[];
// - verification key for the signatures the issuer provides over its Signed
// Redemption Records (SRRs)
extern const char kTrustTokenKeyCommitmentSrrkeyField[];
// - each issuance key's expiry timestamp
extern const char kTrustTokenKeyCommitmentExpiryField[];
// - each issuance key's key material
extern const char kTrustTokenKeyCommitmentKeyField[];

class TrustTokenKeyCommitmentParser
    : public TrustTokenKeyCommitmentController::Parser {
 public:
  TrustTokenKeyCommitmentParser() = default;
  ~TrustTokenKeyCommitmentParser() override = default;

  // Parses a JSON key commitment response.
  //
  // This method returns nullptr unless:
  // - the input is valid JSON; and
  // - the JSON represents a nonempty dictionary; and
  // - within this inner dictionary (which stores metadata like batch size, as
  // well as more dictionaries denoting keys' information):
  //   - every dictionary-type value has an expiry field
  //   (|kTrustTokenKeyCommitmentExpiryField| above) and a key body field
  //   (|kTrustTokenKeyCommitmentKeyField|), and
  //   - the expiry field is a positive integer (microseconds since the Unix
  //   epoch) storing a time in the future.
  mojom::TrustTokenKeyCommitmentResultPtr Parse(
      base::StringPiece response_body) override;

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
  ParseMultipleIssuers(base::StringPiece response_body);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_
