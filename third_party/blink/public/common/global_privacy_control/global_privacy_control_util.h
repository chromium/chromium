// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_GLOBAL_PRIVACY_CONTROL_GLOBAL_PRIVACY_CONTROL_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_GLOBAL_PRIVACY_CONTROL_GLOBAL_PRIVACY_CONTROL_UTIL_H_

#include <string_view>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(GPCSignalSourceType)
enum class GPCSignalSourceType {
  kWorkerNavigation = 0,
  kFrameNavigation = 1,
  kSubresourceFetch = 2,
  kWorkerSubresourceFetch = 3,
  kMaxValue = kWorkerSubresourceFetch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:GPCSignalSourceType)

// Returns true if the Global Privacy Control is enabled.
BLINK_COMMON_EXPORT bool IsGlobalPrivacyControlEnabled();

// Records the source of the GPC signal in a subsampled histogram.
// The histogram is subsampled one out of one thousand times.
BLINK_COMMON_EXPORT void MaybeRecordGlobalPrivacyControlSourceMetric(
    GPCSignalSourceType source_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_GLOBAL_PRIVACY_CONTROL_GLOBAL_PRIVACY_CONTROL_UTIL_H_
