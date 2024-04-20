// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_priority_util.h"

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

double CalculateInterestGroupPriority(
    const blink::AuctionConfig& auction_config,
    const StorageInterestGroup& storage_interest_group,
    const base::Time auction_start_time,
    const base::flat_map<std::string, double>& priority_vector,
    std::optional<double> first_dot_product_priority) {
  // Empty priority vectors should be ignored, so updating an interest group can
  // set an empty priority vector to disable it.
  DCHECK(!priority_vector.empty());

  const blink::InterestGroup& interest_group =
      storage_interest_group.interest_group;

  // Calculate the time since the interest group was joined. Force it to be
  // between 0 and 30 days. Less than 0 could happen due to clock changes. The
  // expiration time is 30 days, but could exceed it due to races or clock
  // changes in the middle of an auction, though seems unlikely.
  base::TimeDelta age = auction_start_time - storage_interest_group.join_time;
  age = std::max(age, base::TimeDelta());
  age = std::min(age, base::Days(30));

  // TODO(crbug.com/40231372): Add browserSignals.age.
  base::flat_map<std::string, double> browser_signals{
      {"browserSignals.one", 1},
      {"browserSignals.basePriority", interest_group.priority},
      {"browserSignals.firstDotProductPriority",
       first_dot_product_priority.value_or(0)},
      {"browserSignals.ageInMinutes", age.InMinutes()},
      {"browserSignals.ageInMinutesMax60", std::min(age.InMinutes(), 60)},
      {"browserSignals.ageInHoursMax24", std::min(age.InHours(), 24)},
      {"browserSignals.ageInDaysMax30", std::min(age.InDays(), 30)},
  };

  // Ordered list of priority signals map. Order is
  // 1) `interest_group.priority_signals_overrides`
  // 2) `browser_signals`
  // 3) `auction_config.per_buyer_priority_signals`
  // 4) `auction_config.all_buyers_priority_signals`
  //
  //  All values except `browser_signals` may not be present. `browser_signals`
  // does not technically have to be in front of the auction config values,
  // since auction configs are prevented from overriding its values by the
  // AuctionConfig typemap.
  std::vector<const base::flat_map<std::string, double>*> maps;
  maps.reserve(4);
  if (interest_group.priority_signals_overrides)
    maps.push_back(&interest_group.priority_signals_overrides.value());
  maps.push_back(&browser_signals);
  if (auction_config.non_shared_params.per_buyer_priority_signals) {
    auto per_buyer_priority_signals =
        auction_config.non_shared_params.per_buyer_priority_signals->find(
            interest_group.owner);
    if (per_buyer_priority_signals !=
        auction_config.non_shared_params.per_buyer_priority_signals->end()) {
      maps.push_back(&per_buyer_priority_signals->second);
    }
  }
  if (auction_config.non_shared_params.all_buyers_priority_signals) {
    maps.push_back(
        &auction_config.non_shared_params.all_buyers_priority_signals.value());
  }

  double caclulated_priority = 0;
  for (const auto& priority_pair : priority_vector) {
    for (const auto* map : maps) {
      const auto signals_pair = map->find(priority_pair.first);
      if (signals_pair == map->end())
        continue;
      caclulated_priority += signals_pair->second * priority_pair.second;
      break;
    }
  }
  return caclulated_priority;
}

}  // namespace content
