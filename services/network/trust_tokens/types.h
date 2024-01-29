// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "url/origin.h"

namespace network {
namespace internal {

// types.h provides utility functions for Trust TrustTokens type conversion.

// Converts a Timstamp into a base:Time.
base::Time TimestampToTime(Timestamp timestamp);

// Converts a base:Time into a Timestamp.
Timestamp TimeToTimestamp(base::Time time);

// Serializes a TrustTokenOperationType.
std::string_view TrustTokenOperationTypeToString(
    mojom::TrustTokenOperationType type);

// Serializes a mojom::TrustTokenProtocolVersion.
std::string ProtocolVersionToString(mojom::TrustTokenProtocolVersion);

}  // namespace internal
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TYPES_H_
