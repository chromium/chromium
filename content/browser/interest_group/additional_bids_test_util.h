// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_TEST_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_TEST_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace content {

enum class SignedAdditionalBidFault {
  // Produce proper encoded signed additional bid.
  kNone,

  // What's supposed to be base64-encoded signed additional bid isn't base 64.
  kInvalidSignedBase64,

  // What's supposed to be signed additional bid isn't JSON.
  kInvalidSignedJson,

  // Signed additional bid JSON doesn't have the right structure.
  kInvalidSignedBidStructure,

  // Everything looks right, but the signature does not verify.
  kInvalidSignature,

  // Everything looks right, but the signature does not verify for only the
  // first signature.
  kOneInvalidSignature,
};

// Generates value of Ad-Auction-Additional-Bid header for nonce `nonce`,
// with bid `base64_bid_string` and ED25519 private keys in `private_keys`.
// The resulting header will claim that `base64_public_keys` can be used to
// verify the signature (so it is useful to test both with them matching and
// mismatching the `private_keys`).
std::string GenerateSignedAdditionalBidHeader(
    SignedAdditionalBidFault inject_fault,
    const std::string& nonce,
    const std::string& bid_string,
    const std::vector<const uint8_t*>& private_keys,
    const std::vector<std::string>& base64_public_keys);

// Generates just the payload portion (e.g. stuff after Nonce:) of
// Ad-Auction-Additional-Bid.
std::string GenerateSignedAdditionalBidHeaderPayloadPortion(
    SignedAdditionalBidFault inject_fault,
    const std::string& bid_string,
    const std::vector<const uint8_t*>& private_keys,
    const std::vector<std::string>& base64_public_keys);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_TEST_UTIL_H_
