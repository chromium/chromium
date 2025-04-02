// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_TEST_UTILS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_TEST_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_pmt_report_util.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace content::PrivateModelTrainingTestUtils {

class TestHpkeKey {
 public:
  // Generates a new HPKE key. Note that `key_id` is just a label.
  explicit TestHpkeKey(std::string key_id = "example_id");
  ~TestHpkeKey();

  // This class is move-only.
  TestHpkeKey(TestHpkeKey&&);
  TestHpkeKey& operator=(TestHpkeKey&&);
  TestHpkeKey(TestHpkeKey&) = delete;
  TestHpkeKey& operator=(TestHpkeKey&) = delete;

  std::string_view key_id() const { return key_id_; }
  const EVP_HPKE_KEY& full_hpke_key() const { return *full_hpke_key_.get(); }
  BiddingAndAuctionServerKey GetPublicKey() const;

 private:
  std::string key_id_;
  bssl::ScopedEVP_HPKE_KEY full_hpke_key_;
};

std::optional<std::vector<uint8_t>> ExtractAndDecryptFramedPayloadFromCbor(
    const std::vector<uint8_t>& cbor_data,
    const EVP_HPKE_KEY& hpke_private_key);

}  // namespace content::PrivateModelTrainingTestUtils

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_TEST_UTILS_H_
