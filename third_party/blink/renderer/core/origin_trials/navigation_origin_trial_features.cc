// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides GetNavigationOriginTrialFeatures which is declared in
// origin_trials.h. GetNavigationOriginTrialFeatures is defined in this file
// since changes to it require review from security reviewers, listed in the
// SECURITY_OWNERS file.

#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"

namespace blink {

namespace origin_trials {

const HashSet<OriginTrialFeature>& GetNavigationOriginTrialFeatures() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      HashSet<OriginTrialFeature>, navigation_origin_trial_features,
      ({// Enable the kOriginTrialsSampleAPINavigation feature as a navigation
        // feature, for tests.
        OriginTrialFeature::kOriginTrialsSampleAPINavigation,
        OriginTrialFeature::kTextFragmentIdentifiers}));
  return navigation_origin_trial_features;
}

}  // namespace origin_trials

}  // namespace blink
