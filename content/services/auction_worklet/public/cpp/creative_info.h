// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CREATIVE_INFO_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CREATIVE_INFO_H_

#include <optional>
#include <string>

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom-forward.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/origin.h"

namespace auction_worklet {

// Info about a creative, either ad or component ad, that's sent to trusted
// scoring signals server, corresponding to one chosen by a generateBid()
// invocation. `buyer_and_seller_reporting_id` is only applicable, and only
// sent, for ads - not for ad components - as ad components may not provide a
// value for `buyer_and_seller_reporting_id` or any other reporting IDs.
//
// If operating with `send_creative_scanning_metadata` true, the same URL may
// need to be repeated, in cases like it occurring in multiple interest groups
// with the same ad creative but different scanning metadata.
//
// When `send_creative_scanning_metadata` is false, all fields other than
// `ad_descriptor`'s `url` must be kept empty to avoid needlessly duplicating
// URLs.
struct CONTENT_EXPORT CreativeInfo {
  CreativeInfo();
  CreativeInfo(blink::AdDescriptor ad_descriptor,
               std::string creative_scanning_metadata,
               std::optional<url::Origin> interest_group_owner,
               std::string buyer_and_seller_reporting_id);
  CreativeInfo(bool send_creative_scanning_metadata,
               const mojom::CreativeInfoWithoutOwner& mojo_creative_info,
               const url::Origin& in_interest_group_owner,
               const std::optional<std::string>&
                   browser_signal_buyer_and_seller_reporting_id);
  ~CreativeInfo();

  CreativeInfo(CreativeInfo&&);
  CreativeInfo(const CreativeInfo&);
  CreativeInfo& operator=(CreativeInfo&&);
  CreativeInfo& operator=(const CreativeInfo&);

  bool operator<(const CreativeInfo& other) const;

  // The ad and size selected by generateBid().
  blink::AdDescriptor ad_descriptor;

  // From `InterestGroup::Ad::creative_scanning_metadata`, with nullopt
  // converted to empty string.
  std::string creative_scanning_metadata;

  // From `InterestGroup::owner`.
  std::optional<url::Origin> interest_group_owner;

  // From `InterestGroup::Ad::buyer_and_seller_reporting_id`, with nullopt
  // converted to empty string.
  std::string buyer_and_seller_reporting_id;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CREATIVE_INFO_H_
