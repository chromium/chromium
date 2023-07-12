// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides FeatureHasExpiryGracePeriod which is declared in
// origin_trials.h. FeatureHasExpiryGracePeriod is defined in this file since
// changes to it require review from the origin trials team, listed in the
// OWNERS file.

#include "third_party/blink/public/common/origin_trials/origin_trials.h"

#include "base/containers/contains.h"

namespace blink::origin_trials {

bool FeatureHasExpiryGracePeriod(OriginTrialFeature feature) {
  static OriginTrialFeature const kHasExpiryGracePeriod[] = {
      // Enable the kOriginTrialsSampleAPI* features as a manual completion
      // features, for tests.
      OriginTrialFeature::kOriginTrialsSampleAPIExpiryGracePeriod,
      OriginTrialFeature::kOriginTrialsSampleAPIExpiryGracePeriodThirdParty,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentExpiryGracePeriod,
      // Production grace period trials start here:
      OriginTrialFeature::kWebViewXRequestedWithDeprecation,
  };
  return base::Contains(kHasExpiryGracePeriod, feature);
}

}  // namespace blink::origin_trials
