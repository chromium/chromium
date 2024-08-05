// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"

namespace auction_worklet {

inline constexpr auto kReservedEventTypes =
    base::MakeFixedFlatMap<std::string_view,
                           auction_worklet::mojom::ReservedEventType>(
        {{"reserved.always",
          auction_worklet::mojom::ReservedEventType::kReservedAlways},
         {"reserved.win",
          auction_worklet::mojom::ReservedEventType::kReservedWin},
         {"reserved.loss",
          auction_worklet::mojom::ReservedEventType::kReservedLoss}});

std::optional<auction_worklet::mojom::ReservedEventType> ParseReservedEventType(
    const std::string& type);

std::optional<auction_worklet::mojom::EventTypePtr>
ParsePrivateAggregationEventType(const std::string& event_type_str);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_
