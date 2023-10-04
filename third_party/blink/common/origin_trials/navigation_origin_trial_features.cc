// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides FeatureEnabledForNavigation which is declared in
// origin_trials.h. FeatureEnabledForNavigation is defined in this file since
// changes to it require review from security reviewers, listed in the
// SECURITY_OWNERS file.

#include "third_party/blink/public/common/origin_trials/origin_trials.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"

namespace blink {

namespace origin_trials {

bool FeatureEnabledForNavigation(blink::mojom::OriginTrialFeature feature) {
  static blink::mojom::OriginTrialFeature const kEnabledForNavigation[] = {
      // Enable the kOriginTrialsSampleAPINavigation feature as a navigation
      // feature, for tests.
      blink::mojom::OriginTrialFeature::kOriginTrialsSampleAPINavigation,
      blink::mojom::OriginTrialFeature::kTextFragmentIdentifiers,
  };
  return base::Contains(kEnabledForNavigation, feature);
}

}  // namespace origin_trials

}  // namespace blink
