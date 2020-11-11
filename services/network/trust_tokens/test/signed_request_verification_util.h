// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "url/gurl.h"

namespace network {
namespace test {

// Reconstructs a request's canonical request data, extracts the signatures from
// its Sec-Signature header, checks that the Sec-Signature header's contained
// signatures verify.
//
// Optionally:
// - If |verification_keys_out| is non-null, on success, returns the
// verification key for each issuer, so that the caller can verify further state
// concerning the key (like confirming that the key was bound to a previous
// redemption).
// - If |error_out| is non-null, on failure, sets it to a human-readable
// description of the reason the verification failed.
// - If |verifier| is non-null, uses the given verifier to verify the
// signatures instead of Ed25519.
bool ReconstructSigningDataAndVerifySignatures(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::RepeatingCallback<bool(base::span<const uint8_t> data,
                                 base::span<const uint8_t> signature,
                                 base::span<const uint8_t> verification_key,
                                 const std::string& sig_alg)> verifier =
        {},  // defaults to Ed25519
    std::string* error_out = nullptr,
    std::map<std::string, std::string>* verification_keys_out = nullptr,
    mojom::TrustTokenSignRequestData* sign_request_data_out = nullptr);

// Parses a Sec-Redemption-Record header and extracts the (issuer, redemption
// record) pairs the header contains. On success, returns true. On
// failure, returns false and, if |error_out| is not null, stores a
// helpful error message in |error_out| for debugging.
bool ExtractRedemptionRecordsFromHeader(
    base::StringPiece sec_redemption_record_header,
    std::map<SuitableTrustTokenOrigin, std::string>*
        redemption_records_per_issuer_out,
    std::string* error_out);

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
