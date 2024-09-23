// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"

#include <optional>

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace {

constinit std::optional<bool> g_enabled;

}  // namespace

namespace blink {

bool LcppEnabled() {
  if (!g_enabled.has_value()) {
    g_enabled =
        base::FeatureList::IsEnabled(
            blink::features::kLCPCriticalPathPredictor) ||
        base::FeatureList::IsEnabled(blink::features::kLCPScriptObserver) ||
        base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor) ||
        base::FeatureList::IsEnabled(
            blink::features::kLCPPLazyLoadImagePreload) ||
        base::FeatureList::IsEnabled(
            blink::features::kDelayAsyncScriptExecution) ||
        base::FeatureList::IsEnabled(
            blink::features::kHttpDiskCachePrewarming) ||
        base::FeatureList::IsEnabled(
            blink::features::kLCPPAutoPreconnectLcpOrigin) ||
        base::FeatureList::IsEnabled(
            blink::features::kLCPTimingPredictorPrerender2) ||
        base::FeatureList::IsEnabled(blink::features::kLCPPDeferUnusedPreload);
  }

  return *g_enabled;
}

void ResetLcppEnabledForTesting() {
  g_enabled.reset();
}

bool LcppScriptObserverEnabled() {
  static const bool enabled =
      base::FeatureList::IsEnabled(blink::features::kLCPScriptObserver) ||
      (base::FeatureList::IsEnabled(
           features::kLowPriorityAsyncScriptExecution) &&
       features::kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam
           .Get());
  return enabled;
}

}  // namespace blink
