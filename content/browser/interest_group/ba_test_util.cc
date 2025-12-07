// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ba_test_util.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/public/test/privacy_sandbox_coordinator_test_util.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/origin.h"

namespace content {

JoinBidWinHistoryForTest::JoinBidWinHistoryForTest() = default;
JoinBidWinHistoryForTest::~JoinBidWinHistoryForTest() = default;
JoinBidWinHistoryForTest::JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&) =
    default;
JoinBidWinHistoryForTest& JoinBidWinHistoryForTest::operator=(
    JoinBidWinHistoryForTest&) = default;
JoinBidWinHistoryForTest::JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&&) =
    default;
JoinBidWinHistoryForTest& JoinBidWinHistoryForTest::operator=(
    JoinBidWinHistoryForTest&&) = default;

std::map<std::string, JoinBidWinHistoryForTest> ExtractJoinBidWinHistories(
    const std::string_view bna_request,
    const url::Origin& bidder) {
  std::map<std::string, JoinBidWinHistoryForTest> result;

  // Decrypt the message.
  CHECK(!bna_request.empty());

  auto key_config =
      quiche::ObliviousHttpHeaderKeyConfig::Create(
          kTestPrivacySandboxCoordinatorId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
          EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM)
          .value();
  auto ohttp_gateway =
      quiche::ObliviousHttpGateway::Create(
          GetTestPrivacySandboxCoordinatorPrivateKey(), key_config)
          .value();
  EXPECT_EQ(0x00, bna_request[0]);
  auto request = ohttp_gateway.DecryptObliviousHttpRequest(
      bna_request.substr(1), kBiddingAndAuctionEncryptionRequestMediaType);
  CHECK(request.ok()) << request.status();
  auto plaintext_data = request->GetPlaintextData();

  EXPECT_EQ(0x02, plaintext_data[0]);
  size_t request_size = 0;
  for (size_t idx = 0; idx < sizeof(uint32_t); idx++) {
    request_size =
        (request_size << 8) | static_cast<uint8_t>(plaintext_data[idx + 1]);
  }

  // Parse the top-level message and find `bidder` in interestGroups.
  cbor::Value top_level_message =
      cbor::Reader::Read(
          base::as_byte_span(plaintext_data.substr(5, request_size)))
          .value();
  cbor::Value::BinaryValue compressed_groups =
      top_level_message.GetMap()
          .at(cbor::Value("interestGroups"))
          .GetMap()
          .at(cbor::Value(bidder.Serialize()))
          .GetBytestring();

  // Decompress the interest groups of `bidder`.
  std::vector<uint8_t> uncompressed_groups_buf(
      compression::GetUncompressedSize(compressed_groups));
  CHECK(compression::GzipUncompress(/*input=*/compressed_groups,
                                    /*output=*/uncompressed_groups_buf));

  // Parse the interest groups of `bidder` and pull out their browser signals.
  cbor::Value uncompressed_groups =
      cbor::Reader::Read(uncompressed_groups_buf).value();
  for (const cbor::Value& group_val : uncompressed_groups.GetArray()) {
    const cbor::Value::MapValue& group = group_val.GetMap();
    std::string name = group.at(cbor::Value("name")).GetString();
    JoinBidWinHistoryForTest join_bid_win_history;

    const cbor::Value::MapValue& browser_signals =
        group.at(cbor::Value("browserSignals")).GetMap();
    join_bid_win_history.join_count =
        browser_signals.at(cbor::Value("joinCount")).GetInteger();
    join_bid_win_history.bid_count =
        browser_signals.at(cbor::Value("bidCount")).GetInteger();

    for (const cbor::Value& prev_wins_val :
         browser_signals.at(cbor::Value("prevWins")).GetArray()) {
      JoinBidWinHistoryForTest::PrevWin prev_win;
      const cbor::Value::ArrayValue& prev_wins = prev_wins_val.GetArray();
      prev_win.prev_win_time_seconds = prev_wins.at(0).GetInteger();
      prev_win.ad_render_id = prev_wins.at(1).GetString();
      join_bid_win_history.prev_wins.emplace_back(std::move(prev_win));
    }

    result[name] = std::move(join_bid_win_history);
  }

  return result;
}

}  // namespace content
