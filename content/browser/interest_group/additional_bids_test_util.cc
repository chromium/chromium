// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/additional_bids_test_util.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace content {

std::string GenerateSignedAdditionalBidHeaderPayloadPortion(
    SignedAdditionalBidFault inject_fault,
    const std::string& bid_string,
    const std::vector<const uint8_t*>& private_keys,
    const std::vector<std::string>& base64_public_keys) {
  CHECK_EQ(private_keys.size(), base64_public_keys.size());

  base::Value::Dict signed_additional_bid;
  signed_additional_bid.Set("bid", bid_string);

  base::Value::List signatures_list;

  std::string bid_string_to_sign = bid_string;
  if (inject_fault == SignedAdditionalBidFault::kInvalidSignature ||
      inject_fault == SignedAdditionalBidFault::kOneInvalidSignature) {
    bid_string_to_sign = "something completely different";
  }

  for (size_t i = 0; i < private_keys.size(); ++i) {
    uint8_t sig[64];
    bool ok = ED25519_sign(
        sig, reinterpret_cast<const uint8_t*>(bid_string_to_sign.data()),
        bid_string_to_sign.size(), private_keys[i]);
    CHECK(ok);
    base::Value::Dict sig_dict;
    sig_dict.Set("key", base64_public_keys[i]);
    sig_dict.Set("signature", base::Base64Encode(sig));
    signatures_list.Append(std::move(sig_dict));

    if (inject_fault == SignedAdditionalBidFault::kOneInvalidSignature) {
      bid_string_to_sign = bid_string;
    }
  }

  signed_additional_bid.Set("signatures", std::move(signatures_list));

  if (inject_fault == SignedAdditionalBidFault::kInvalidSignedBidStructure) {
    signed_additional_bid.Remove("bid");
  }

  std::string signed_additional_bid_str =
      base::WriteJson(signed_additional_bid).value();
  if (inject_fault == SignedAdditionalBidFault::kInvalidSignedJson) {
    signed_additional_bid_str = "}" + signed_additional_bid_str;
  }

  std::string encoded_signed_additional_bid =
      base::Base64Encode(signed_additional_bid_str);
  // Prepend some whitespace to make sure the decoder is forgiving.
  encoded_signed_additional_bid = " " + encoded_signed_additional_bid;

  if (inject_fault == SignedAdditionalBidFault::kInvalidSignedBase64) {
    encoded_signed_additional_bid = "??" + encoded_signed_additional_bid;
  }

  return encoded_signed_additional_bid;
}

std::string GenerateSignedAdditionalBidHeader(
    SignedAdditionalBidFault inject_fault,
    const std::string& nonce,
    const std::string& bid_string,
    const std::vector<const uint8_t*>& private_keys,
    const std::vector<std::string>& base64_public_keys) {
  return base::StrCat(
      {nonce, ":",
       GenerateSignedAdditionalBidHeaderPayloadPortion(
           inject_fault, bid_string, private_keys, base64_public_keys)});
}

}  // namespace content
