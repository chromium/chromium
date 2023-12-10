// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

bool LcppEnabled() {
  return base::FeatureList::IsEnabled(
             blink::features::kLCPCriticalPathPredictor) ||
         base::FeatureList::IsEnabled(blink::features::kLCPScriptObserver) ||
         base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor) ||
         base::FeatureList::IsEnabled(
             blink::features::kLCPPLazyLoadImagePreload) ||
         base::FeatureList::IsEnabled(
             blink::features::kDelayAsyncScriptExecution);
}

}  // namespace blink
