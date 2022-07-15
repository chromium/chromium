// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"

namespace content {

// InterestGroupUpdate represents the results of parsing a JSON update for a
// stored blink::InterestGroup file. It contains all updatable fields of a
// InterestGroup - that is, everything but `name`, `origin`, `expiry`, and
// `user_bidding_signals`. All fields are optional, even ones that are mandatory
// in an InterestGroup, since the value of the original InterestGroup will be
// used when they're not present in an InterestGroupUpdate.
struct CONTENT_EXPORT InterestGroupUpdate {
  InterestGroupUpdate();
  InterestGroupUpdate(const InterestGroupUpdate&);
  InterestGroupUpdate(InterestGroupUpdate&&);
  ~InterestGroupUpdate();

  absl::optional<double> priority;
  absl::optional<blink::InterestGroup::ExecutionMode> execution_mode;
  absl::optional<GURL> bidding_url;
  absl::optional<GURL> bidding_wasm_helper_url;
  absl::optional<GURL> daily_update_url;
  absl::optional<GURL> trusted_bidding_signals_url;
  absl::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  absl::optional<std::vector<blink::InterestGroup::Ad>> ads, ad_components;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
