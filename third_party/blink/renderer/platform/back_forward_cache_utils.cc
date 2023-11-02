// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool IsInflightNetworkRequestBackForwardCacheSupportEnabled() {
  // Note that the call to RuntimeEnabledFeatures::BackForwardCacheEnabled()
  // must be done first to ensure we will never call
  // base::FeatureList::IsEnabled(features::kLoadingTasksUnfreezable) when
  // back-forward cache is not enabled. This is important because IsEnabled()
  // might trigger activation of the current user in BackForwardCache's field
  // trial group even though it shouldn't (e.g. when BackForwardCache is
  // disabled due to low RAM), lowering the back-forward cache hit rate.
  // TODO(rakina): Remove BackForwardCache from RuntimeEnabledFeatures and move
  // features::kBackForwardCache and BackForwardCacheMemoryControls from
  // content/ to blink/public, so that we can combine this check with the checks
  // in content/.
  return RuntimeEnabledFeatures::BackForwardCacheEnabled() &&
         base::FeatureList::IsEnabled(features::kLoadingTasksUnfreezable);
}

int GetLoadingTasksUnfreezableParamAsInt(const std::string& param_name,
                                         int default_value) {
  if (!IsInflightNetworkRequestBackForwardCacheSupportEnabled())
    return default_value;
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kLoadingTasksUnfreezable, param_name, default_value);
}

}  // namespace blink
