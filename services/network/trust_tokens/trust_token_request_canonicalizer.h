// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_CANONICALIZER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_CANONICALIZER_H_

#include <vector>

#include "base/strings/string_piece_forward.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// A TrustTokenRequestCanonicalizer turns a (URLRequest, public key) pair into
// the corresponding "canonical request data," which is a serialized CBOR
// structure comprising the public key and a collection of request data.
//
// Constructing this is a step in the Trust Tokens protocol's request signing
// operation. Exactly what request data is included alongside the public key
// depends on the parameterization of the operation, but it will always include
// a (potentially empty) caller-specified collection of request headers chosen
// from the TrustTokenRequestSigningHelper::kSignableRequestHeaders allowlist.
//
// The normative pseudocode for this operation currently lives in the Trust
// Tokens design doc's "Signature generation" section.
class TrustTokenRequestCanonicalizer {
 public:
  TrustTokenRequestCanonicalizer() = default;
  virtual ~TrustTokenRequestCanonicalizer() = default;

  TrustTokenRequestCanonicalizer(const TrustTokenRequestCanonicalizer&) =
      delete;
  TrustTokenRequestCanonicalizer& operator=(
      const TrustTokenRequestCanonicalizer&) = delete;

  // Attempts to canonicalize a request according to the pseudocode in the
  // design doc's "Signature generation" section, obtaining the headers to sign
  // by inspecting the request's Signed-Headers header. |sign_request_data|'s
  // value denotes whether the signing data should be more (kInclude) or less
  // (kHeadersOnly) descriptive; refer to the normative pseudocode for details.
  //
  // |destination| and |headers| together represent an outgoing request.
  //
  // Returns nullopt if the request's Signed-Headers header is malformed (i.e.,
  // not a valid Structured Headers list of atoms); if |public_key| is empty; or
  // if there is an internal error during serialization.
  //
  // REQUIRES: |sign_request_data| is kInclude or kHeadersOnly.
  virtual absl::optional<std::vector<uint8_t>> Canonicalize(
      const GURL& destination,
      const net::HttpRequestHeaders& headers,
      base::StringPiece public_key,
      mojom::TrustTokenSignRequestData sign_request_data) const;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_CANONICALIZER_H_
