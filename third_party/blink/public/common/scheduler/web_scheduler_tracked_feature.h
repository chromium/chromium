// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/scheduler/web_scheduler_tracked_feature.mojom-shared.h"

namespace blink {
namespace scheduler {

using WebSchedulerTrackedFeature = mojom::WebSchedulerTrackedFeature;

using WebSchedulerTrackedFeatures =
    base::EnumSet<WebSchedulerTrackedFeature,
                  WebSchedulerTrackedFeature::kMinValue,
                  WebSchedulerTrackedFeature::kMaxValue>;

BLINK_COMMON_EXPORT const char* FeatureToHumanReadableString(
    WebSchedulerTrackedFeature feature);
BLINK_COMMON_EXPORT std::string FeatureToShortString(
    WebSchedulerTrackedFeature feature);

BLINK_COMMON_EXPORT std::optional<WebSchedulerTrackedFeature> StringToFeature(
    const std::string& str);
// Returns true if there was previously a feature by this name.
// It is not comprehensive, just enough to cover what was used in finch,
// in order to stop warnings at startup. See https://crbug.com/1363846.
BLINK_COMMON_EXPORT bool IsRemovedFeature(const std::string& feature);

// Sticky features can't be unregistered and remain active for the rest of the
// lifetime of the page.
BLINK_COMMON_EXPORT bool IsFeatureSticky(WebSchedulerTrackedFeature feature);

// All the sticky features.
BLINK_COMMON_EXPORT WebSchedulerTrackedFeatures StickyFeatures();

// Disables wake up alignment permanently for the process. This is called when a
// feature that is incompatible with wake up alignment is used. Thread-safe.
BLINK_COMMON_EXPORT void DisableAlignWakeUpsForProcess();
BLINK_COMMON_EXPORT bool IsAlignWakeUpsDisabledForProcess();

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_
