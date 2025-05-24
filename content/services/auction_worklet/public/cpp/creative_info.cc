// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/creative_info.h"

#include <optional>
#include <string>
#include <tuple>

#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/origin.h"

namespace auction_worklet {

CreativeInfo::CreativeInfo() = default;
CreativeInfo::CreativeInfo(blink::AdDescriptor ad_descriptor,
                           std::string creative_scanning_metadata,
                           std::optional<url::Origin> interest_group_owner,
                           std::string buyer_and_seller_reporting_id)
    : ad_descriptor(std::move(ad_descriptor)),
      creative_scanning_metadata(std::move(creative_scanning_metadata)),
      interest_group_owner(std::move(interest_group_owner)),
      buyer_and_seller_reporting_id(std::move(buyer_and_seller_reporting_id)) {}

CreativeInfo::CreativeInfo(
    bool send_creative_scanning_metadata,
    const mojom::CreativeInfoWithoutOwner& mojo_creative_info,
    const url::Origin& in_interest_group_owner,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id) {
  ad_descriptor.url = mojo_creative_info.ad_descriptor.url;
  if (send_creative_scanning_metadata) {
    ad_descriptor.size = mojo_creative_info.ad_descriptor.size;
    creative_scanning_metadata =
        mojo_creative_info.creative_scanning_metadata.value_or(std::string());
    interest_group_owner = in_interest_group_owner;
    buyer_and_seller_reporting_id =
        browser_signal_buyer_and_seller_reporting_id.value_or(std::string());
  }
}

CreativeInfo::~CreativeInfo() = default;

CreativeInfo::CreativeInfo(CreativeInfo&&) = default;
CreativeInfo::CreativeInfo(const CreativeInfo&) = default;
CreativeInfo& CreativeInfo::operator=(CreativeInfo&&) = default;
CreativeInfo& CreativeInfo::operator=(const CreativeInfo&) = default;

bool CreativeInfo::operator<(const CreativeInfo& other) const {
  return std::tie(ad_descriptor, creative_scanning_metadata,
                  interest_group_owner, buyer_and_seller_reporting_id) <
         std::tie(other.ad_descriptor, other.creative_scanning_metadata,
                  other.interest_group_owner,
                  other.buyer_and_seller_reporting_id);
}

}  // namespace auction_worklet
