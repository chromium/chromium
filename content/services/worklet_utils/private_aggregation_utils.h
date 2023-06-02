// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_WORKLET_UTILS_PRIVATE_AGGREGATION_UTILS_H_
#define CONTENT_SERVICES_WORKLET_UTILS_PRIVATE_AGGREGATION_UTILS_H_

#include <string>

#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-forward.h"
#include "v8/include/v8-primitive.h"

namespace gin {
class Arguments;
}

namespace worklet_utils {

// If returns `absl::nullopt`, will output an error to `error_out`.
absl::optional<absl::uint128> ConvertBigIntToUint128(
    v8::Local<v8::BigInt> bigint,
    std::string* error_out);

// Parses arguments provided to `contributeToHistogram()` and returns the
// corresponding contribution. In case of an error, throws an exception and
// returns `nullptr`.
blink::mojom::AggregatableReportHistogramContributionPtr
ParseContributeToHistogramArguments(
    const gin::Arguments& args,
    bool private_aggregation_permissions_policy_allowed);

// Parses arguments provided to `enableDebugMode()` and updates
// `debug_mode_details` as appropriate. `debug_mode_details` must be passed a
// reference to the existing (likely default) details. In case of an error,
// throws an exception and does not update `debug_mode_details`.
void ParseAndApplyEnableDebugModeArguments(
    const gin::Arguments& args,
    bool private_aggregation_permissions_policy_allowed,
    blink::mojom::DebugModeDetails& debug_mode_details);

}  // namespace worklet_utils

#endif  // CONTENT_SERVICES_WORKLET_UTILS_PRIVATE_AGGREGATION_UTILS_H_
