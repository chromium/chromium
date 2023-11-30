// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/types.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "services/network/trust_tokens/proto/public.pb.h"

namespace network::internal {

base::Time TimestampToTime(Timestamp timestamp) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(timestamp.micros()));
}

Timestamp TimeToTimestamp(base::Time time) {
  Timestamp timestamp = Timestamp();
  timestamp.set_micros(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return timestamp;
}

std::string_view TrustTokenOperationTypeToString(
    mojom::TrustTokenOperationType type) {
  // WARNING: These values are used to construct histogram names. When making
  // changes, please make sure that the Trust Tokens-related histograms
  // ("Net.TrustTokens.*") reflect the changes.
  switch (type) {
    case mojom::TrustTokenOperationType::kIssuance:
      return "Issuance";
    case mojom::TrustTokenOperationType::kRedemption:
      return "Redemption";
    case mojom::TrustTokenOperationType::kSigning:
      return "Signing";
  }
}

std::string ProtocolVersionToString(
    mojom::TrustTokenProtocolVersion my_version) {
  switch (my_version) {
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb:
      return "PrivateStateTokenV3PMB";
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Voprf:
      return "PrivateStateTokenV3VOPRF";
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb:
      return "PrivateStateTokenV1PMB";
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Voprf:
      return "PrivateStateTokenV1VOPRF";
  }
}

}  // namespace network::internal
