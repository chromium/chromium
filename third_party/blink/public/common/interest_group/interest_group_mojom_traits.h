// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::InterestGroupAdDataView,
                                        blink::InterestGroup::Ad> {
  static const GURL& render_url(const blink::InterestGroup::Ad& ad) {
    return ad.render_url;
  }

  static const absl::optional<std::string>& metadata(
      const blink::InterestGroup::Ad& ad) {
    return ad.metadata;
  }

  static bool Read(blink::mojom::InterestGroupAdDataView data,
                   blink::InterestGroup::Ad* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::InterestGroupDataView, blink::InterestGroup> {
  static base::Time expiry(const blink::InterestGroup& interest_group) {
    return interest_group.expiry;
  }

  static const url::Origin& owner(const blink::InterestGroup& interest_group) {
    return interest_group.owner;
  }

  static const std::string& name(const blink::InterestGroup& interest_group) {
    return interest_group.name;
  }

  static const absl::optional<GURL>& bidding_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.bidding_url;
  }

  static const absl::optional<GURL>& bidding_wasm_helper_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.bidding_wasm_helper_url;
  }

  static const absl::optional<GURL>& update_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.update_url;
  }

  static const absl::optional<GURL>& trusted_bidding_signals_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.trusted_bidding_signals_url;
  }

  static const absl::optional<std::vector<std::string>>&
  trusted_bidding_signals_keys(const blink::InterestGroup& interest_group) {
    return interest_group.trusted_bidding_signals_keys;
  }

  static const absl::optional<std::string>& user_bidding_signals(
      const blink::InterestGroup& interest_group) {
    return interest_group.user_bidding_signals;
  }

  static const absl::optional<std::vector<blink::InterestGroup::Ad>>& ads(
      const blink::InterestGroup& interest_group) {
    return interest_group.ads;
  }

  static const absl::optional<std::vector<blink::InterestGroup::Ad>>&
  ad_components(const blink::InterestGroup& interest_group) {
    return interest_group.ad_components;
  }

  static bool Read(blink::mojom::InterestGroupDataView data,
                   blink::InterestGroup* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_
