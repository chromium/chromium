// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_CLIENT_DATA_CANONICALIZATION_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_CLIENT_DATA_CANONICALIZATION_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "url/origin.h"

namespace network {

// Returns a CBOR serialization of the "client-data" field for a Trust Tokens
// redemption request, given a redemption timestamp; and a redeeming top-frame
// origin.
//
// Follows the format specified in the Trust Tokens design doc (currently the
// normative source for such things), at
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#
//
// Returns nullopt if |redemption_timestamp| is earlier than the Unix epoch, or
// on serialization error.
//
// |top_frame_origin| must not be opaque.
std::optional<std::vector<uint8_t>>
CanonicalizeTrustTokenClientDataForRedemption(
    base::Time redemption_timestamp,
    const url::Origin& top_frame_origin);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_CLIENT_DATA_CANONICALIZATION_H_
