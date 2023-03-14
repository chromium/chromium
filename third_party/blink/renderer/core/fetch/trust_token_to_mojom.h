// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_TO_MOJOM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_TO_MOJOM_H_

#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class DOMException;
class PrivateToken;

// Converts an IDL trustToken object to its Mojo counterpart.
// The elements of trustToken (and of TrustTokenParams) comprise:
// - a token type, always populated
// - a version type, always populated
// - an operation type, always populated
// - remaining elements partitioned into groups of parameters used for specific
// operations.
//
// The method sets |type|, |version|, |operation| and the fields corresponding
// to the operation specified by |operation|, namely
// - for issuance, no additional fields;
// - for redemption, |refresh_policy|;
// - for signing: |issuer|, |additional_signed_headers|, |sign_request_data|,
// and |include_timestamp_header|.
//
// Performs some validity checking against inputs:
// - for signing, |issuer| must be provided and must be a valid HTTP(S) URL.
// If this validation fails, throws a TypeError against |exception_state| and
// returns false.
bool ConvertTrustTokenToMojom(const PrivateToken& in,
                              ExceptionState* exception_state,
                              network::mojom::blink::TrustTokenParams* out);

// Converts a Mojo TrustTokenOperationStatus denoting an error into a
// DOMException suitable for displaying to the API's client.
//
// This should only be called on failure; |status| must not equal kOk.
DOMException* TrustTokenErrorToDOMException(
    network::mojom::blink::TrustTokenOperationStatus error);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_TO_MOJOM_H_
