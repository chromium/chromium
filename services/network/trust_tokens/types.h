// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_

#include <string>

#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "url/origin.h"

namespace network {
namespace internal {

// types.h provides utility functions for Trust TrustTokens type conversion.

// Deserializes a base::Time. Returns nullopt on failure (for instance,
// deserialization can fail if |my_string| is malformed due to data
// corruption) and the deserialized Time on success.
base::Optional<base::Time> StringToTime(base::StringPiece my_string);

// Serializes a base::Time.
std::string TimeToString(base::Time my_time);

// Serializes a TrustTokenOperationType.
base::StringPiece TrustTokenOperationTypeToString(
    mojom::TrustTokenOperationType type);

// Serializes a mojom::TrustTokenProtocolVersion.
std::string ProtocolVersionToString(mojom::TrustTokenProtocolVersion);

}  // namespace internal
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_
