// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides FeatureHasExpiryGracePeriod which is declared in
// origin_trials.h. FeatureHasExpiryGracePeriod is defined in this file since
// changes to it require review from the origin trials team, listed in the
// OWNERS file.

#include "third_party/blink/public/common/origin_trials/origin_trials.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"

namespace blink::origin_trials {

bool FeatureHasExpiryGracePeriod(blink::mojom::OriginTrialFeature feature) {
  static blink::mojom::OriginTrialFeature const kHasExpiryGracePeriod[] = {
      // Enable the kOriginTrialsSampleAPI* features as a manual completion
      // features, for tests.
      blink::mojom::OriginTrialFeature::kOriginTrialsSampleAPIExpiryGracePeriod,
      blink::mojom::OriginTrialFeature::
          kOriginTrialsSampleAPIExpiryGracePeriodThirdParty,
      blink::mojom::OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentExpiryGracePeriod,
      // Production grace period trials start here:
      blink::mojom::OriginTrialFeature::kWebViewXRequestedWithDeprecation,
      blink::mojom::OriginTrialFeature::kRTCEncodedFrameSetMetadata,
      blink::mojom::OriginTrialFeature::kElementCapture,
      blink::mojom::OriginTrialFeature::kCapturedSurfaceControl,
  };
  return base::Contains(kHasExpiryGracePeriod, feature);
}

}  // namespace blink::origin_trials
