// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "url/origin.h"

namespace content {

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
constexpr inline auto kTestBaPrivateKey = std::to_array<uint8_t>({
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
});

constexpr inline auto kTestBaPublicKey = std::to_array<uint8_t>({
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
});

// Result of extracting join, bid, and win history from the CBOR B&A request.
struct JoinBidWinHistoryForTest {
  struct PrevWin {
    int32_t prev_win_time_seconds;
    std::string ad_render_id;
  };

  JoinBidWinHistoryForTest();
  ~JoinBidWinHistoryForTest();
  JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&);
  JoinBidWinHistoryForTest& operator=(JoinBidWinHistoryForTest&);
  JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&&);
  JoinBidWinHistoryForTest& operator=(JoinBidWinHistoryForTest&&);

  int join_count;
  int bid_count;
  std::vector<PrevWin> prev_wins;
};

// For a given owner, extracts a map from interest group name to the join,
// bid, and win history for that interest group. Crashes if anything goes
// wrong during lookup and extraction.
std::map<std::string, JoinBidWinHistoryForTest> ExtractJoinBidWinHistories(
    std::string_view bna_request,
    const url::Origin& bidder);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_
