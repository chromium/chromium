// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_headers_util.h"

#include <stddef.h>
#include <stdint.h>

#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// This invokes both ParseAdAuctionResultResponseHeader and
// ParseAdAuctionAdditionalBidResponseHeader with the same value.
// Though these have different valid headers, we don't assess the output of
// these functions (as this is a fuzz test), so this simply fuzz tests against
// more inputs.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::test::ScopedFeatureList feature_list{
      blink::features::kFledgeSellerNonce};
  std::string header_value(reinterpret_cast<const char*>(data), size);

  ParseAdAuctionResultResponseHeader(header_value);

  std::map<std::string, std::vector<SignedAdditionalBidWithMetadata>> output;
  ParseAdAuctionAdditionalBidResponseHeader(header_value, output);
  return 0;
}

}  // namespace content
