// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/types.h"

#include "base/time/time.h"
#include "base/util/values/values_util.h"

namespace network {
namespace internal {

base::Optional<base::Time> StringToTime(base::StringPiece my_string) {
  return util::ValueToTime(base::Value(my_string));
}

std::string TimeToString(base::Time my_time) {
  return util::TimeToValue(my_time).GetString();
}

base::StringPiece TrustTokenOperationTypeToString(
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

}  // namespace internal
}  // namespace network
