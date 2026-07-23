// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/global_privacy_control/global_privacy_control_util.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "third_party/blink/public/common/features.h"

namespace blink {
namespace {
// UMA key for GPC source samples.
constexpr char kGlobalPrivacyControlSourceHistogram[] =
    "Network.GlobalPrivacyControlSource.Subsampled";

// Subsampling probability for the GPC source histogram.
inline constexpr double kGlobalPrivacyControlSourceHistogramSampleProbability =
    0.001;
}  // namespace

bool IsGlobalPrivacyControlEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kGlobalPrivacyControlForce);
}

void MaybeRecordGlobalPrivacyControlSourceMetric(
    GPCSignalSourceType source_type) {
  if (base::FeatureList::IsEnabled(
          blink::features::kGlobalPrivacyControlAlwaysSample) ||
      base::ShouldRecordSubsampledMetric(
          kGlobalPrivacyControlSourceHistogramSampleProbability)) {
    UMA_HISTOGRAM_ENUMERATION(kGlobalPrivacyControlSourceHistogram,
                              source_type);
  }
}

}  // namespace blink
