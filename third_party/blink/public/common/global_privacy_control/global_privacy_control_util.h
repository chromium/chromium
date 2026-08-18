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

// Returns true if the feature is enabled, but does not depend on the profile
// setting (or DevTools setting override). Use this to detect if the API is
// available, but not whether to include headers.
// TODO(crbug.com/40745270): `kGlobalPrivacyControlForce` currently enables this
// but it should be removed once we have a real setting to test.
BLINK_COMMON_EXPORT bool IsGlobalPrivacyControlFeatureEnabled();

// Returns true if the feature is enabled and the profile setting (or DevTools
// override) is enabled as well. Use this to decide whether or not to include
// the GPC header, but not to gate API access.
// TODO(crbug.com/40745270): `kGlobalPrivacyControlForce` currently enables this
// but it should be removed once we have a real setting to test.
BLINK_COMMON_EXPORT bool IsGlobalPrivacyControlFeatureAndSettingEnabled();

// Records the source of the GPC signal in a subsampled histogram.
// The histogram is subsampled one out of one thousand times.
BLINK_COMMON_EXPORT void MaybeRecordGlobalPrivacyControlSourceMetric(
    GPCSignalSourceType source_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_GLOBAL_PRIVACY_CONTROL_GLOBAL_PRIVACY_CONTROL_UTIL_H_
