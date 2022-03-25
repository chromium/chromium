// Copyright 2022 The Chromium Authors. All rights reserved.
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
      // Enable the kOriginTrialsSampleAPIExpiryGracePeriod feature as a manual
      // completion feature, for tests.
      OriginTrialFeature::kOriginTrialsSampleAPIExpiryGracePeriod,
  };
  return base::Contains(kHasExpiryGracePeriod, feature);
}

}  // namespace blink::origin_trials
