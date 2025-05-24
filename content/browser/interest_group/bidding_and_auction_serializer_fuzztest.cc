// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "content/browser/interest_group/bidding_and_auction_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace content {

namespace {

struct PerBuyerConfig {
  bool has_config;
  std::optional<uint32_t> requested_size;
  uint32_t estimated_size;
};

struct Config {
  uint32_t request_size;
  std::vector<PerBuyerConfig> per_buyer_configs;
};

void DoesNotCrashForAnyInput(const Config& config) {
  std::vector<std::pair<url::Origin, size_t>> allocated_buyers;
  std::vector<std::pair<url::Origin, size_t>> unallocated_buyers;
  blink::mojom::AuctionDataConfigPtr mojo_config =
      blink::mojom::AuctionDataConfig::New();
  mojo_config->request_size = config.request_size;
  for (size_t idx = 0; idx < config.per_buyer_configs.size(); idx++) {
    url::Origin buyer =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.test/", idx)));
    if (config.per_buyer_configs[idx].has_config) {
      mojo_config->per_buyer_configs[buyer] =
          blink::mojom::AuctionDataBuyerConfig::New(
              config.per_buyer_configs[idx].requested_size);
      if (config.per_buyer_configs[idx].requested_size) {
        allocated_buyers.emplace_back(std::move(buyer), idx);
      } else {
        unallocated_buyers.emplace_back(std::move(buyer), idx);
      }
    } else {
      unallocated_buyers.emplace_back(std::move(buyer), idx);
    }
  }
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*mojo_config);

  for (const auto& buyer : allocated_buyers) {
    estimator.UpdatePerBuyerMaxSize(
        buyer.first, config.per_buyer_configs[buyer.second].estimated_size);
  }
  for (const auto& buyer : unallocated_buyers) {
    estimator.UpdatePerBuyerMaxSize(
        buyer.first, config.per_buyer_configs[buyer.second].estimated_size);
  }

  base::CheckedNumeric<size_t> current_size = 0;
  for (const auto& buyer : allocated_buyers) {
    current_size +=
        estimator.EstimateTargetSize(buyer.first, current_size.ValueOrDie())
            .value_or(0);
  }
  for (const auto& buyer : unallocated_buyers) {
    current_size +=
        estimator.EstimateTargetSize(buyer.first, current_size.ValueOrDie())
            .value_or(0);
  }
  ASSERT_TRUE(current_size.IsValid());
}

FUZZ_TEST(BiddingAndAuctionSerializerTargetSizeEstimator,
          DoesNotCrashForAnyInput);

}  // namespace
}  // namespace content
