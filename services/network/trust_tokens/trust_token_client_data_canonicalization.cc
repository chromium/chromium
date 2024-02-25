// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"

#include "components/cbor/writer.h"
#include "crypto/sha2.h"

namespace network {

namespace {

const char kRedemptionTimestampKey[] = "redemption-timestamp";
const char kRedeemingOriginKey[] = "redeeming-origin";

}  // namespace

// From the design doc:
//
// {
//    // “redemption-timestamp”’s value is of CBOR type “unsigned integer.”
//    “redemption-timestamp”: <Redemption timestamp, seconds past the Unix
//    epoch>,
//
//    // “redeeming-origin”’s value is of CBOR type “text string.”
//    “redeeming-origin”: <Top-level origin at the time of redemption>,
// },
std::optional<std::vector<uint8_t>>
CanonicalizeTrustTokenClientDataForRedemption(
    base::Time redemption_timestamp,
    const url::Origin& top_frame_origin) {
  DCHECK(!top_frame_origin.opaque());

  cbor::Value::MapValue map;

  base::TimeDelta redemption_timestamp_minus_unix_epoch =
      redemption_timestamp - base::Time::UnixEpoch();

  if (redemption_timestamp_minus_unix_epoch.is_negative())
    return std::nullopt;

  map[cbor::Value(kRedemptionTimestampKey, cbor::Value::Type::STRING)] =
      cbor::Value(redemption_timestamp_minus_unix_epoch.InSeconds());

  map[cbor::Value(kRedeemingOriginKey, cbor::Value::Type::STRING)] =
      cbor::Value(top_frame_origin.Serialize(), cbor::Value::Type::STRING);

  return cbor::Writer::Write(cbor::Value(std::move(map)));
}

}  // namespace network
