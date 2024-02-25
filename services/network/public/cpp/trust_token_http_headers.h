// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_HTTP_HEADERS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_HTTP_HEADERS_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"

namespace network {

// These are the HTTP headers defined in the Trust Tokens draft explainer:
// https://github.com/WICG/trust-token-api
//
// NOTE: If you add more request headers, please make sure to update the
// definition of |TrustTokensRequestHeaders|.

// As a request header: during issuance, sends a collection of unsigned, blinded
// tokens; during redemption, sends a single signed, unblinded token
// along with associated redemption metadata.
// As a response header: during issuance, provides a collection of signed,
// blinded tokens; during redemption, includes a just-created Signed Redemption
// Record.
constexpr char kTrustTokensSecTrustTokenHeader[] = "Sec-Private-State-Token";

// As a request header, provides the version of Trust Token being used in the
// Sec-Trust-Token header.
//
// Alongside signed requests, provides the "major" Trust Tokens protocol
// version, for instance "PrivateStateTokenV3" for the protocol versions
// "TrustTokenV3VOPRF" and "TrustTokenV3PMB"). This is a tentative addition to
// make it easy to adapt to a breaking change in the signature payload's format
// without having to feature-detect implicitly by trying detect structural
// chracteristics of the old and new formats: see crbug.com/1209728.
constexpr char kTrustTokensSecTrustTokenVersionHeader[] =
    "Sec-Private-State-Token-Crypto-Version";

// As a request header, provides a timestamp associated with a
// particular Trust Tokens signature-bearing request.
constexpr char kTrustTokensRequestHeaderSecTime[] = "Sec-Time";

// As a request header, provides a signature over the canonical record
// associated with a given request (containing the request's URL; optionally, a
// collection of headers; and, optionally, the request's body).
constexpr char kTrustTokensRequestHeaderSecSignature[] = "Sec-Signature";

// As a request header, provides a Redemption Record obtained from a prior
// issuance-and-redemption flow.
constexpr char kTrustTokensRequestHeaderSecRedemptionRecord[] =
    "Sec-Redemption-Record";

// As a request header during the request signing operation, provides the list
// of headers included in the signing data's canonical request data. An absent
// header denotes an empty list.
constexpr char kTrustTokensRequestHeaderSignedHeaders[] = "Signed-Headers";

// As a request header, provides optional additional client-specified signing
// data alongside signed requests.
constexpr char kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData[] =
    "Sec-Private-State-Tokens-Additional-Signing-Data";

// A response header, from a Trust Token redemption that includes an integer
// representing the lifetime of the Trust Token response, in seconds since the
// redemption. If the header is omitted, the expiry time of the relevant key
// will be used instead.
constexpr char kTrustTokensResponseHeaderSecTrustTokenLifetime[] =
    "Sec-Private-State-Token-Lifetime";

// Returns a view of all of the Trust Tokens-internal request headers.
// This vector contains all of the headers that clients must not provide on
// requests bearing Trust Tokens operations, because they are added internally
// by Trust Tokens logic.
//
// In particular, this does *not* contain Signed-Headers because this header's
// value is provided by the Trust Token API's client.
COMPONENT_EXPORT(NETWORK_CPP)
const std::vector<std::string_view>& TrustTokensRequestHeaders();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_HTTP_HEADERS_H_
